import logging
import time
from typing import Dict, Optional, Set, Tuple

from scaler.io.async_binder import AsyncBinder
from scaler.io.async_connector import AsyncConnector
from scaler.protocol.python.common import ObjectStorageAddress
from scaler.protocol.python.message import (
    ClientDisconnect,
    ClientHeartbeat,
    ClientHeartbeatEcho,
    ClientShutdownResponse,
    TaskCancel,
)
from scaler.protocol.python.status import ClientManagerStatus
from scaler.scheduler.mixins import ClientManager, ObjectManager, TaskManager, WorkerManager
from scaler.utility.exceptions import ClientShutdownException
from scaler.utility.identifiers import ClientID, TaskID
from scaler.utility.mixins import Looper, Reporter
from scaler.utility.one_to_many_dict import OneToManyDict


class VanillaClientManager(ClientManager, Looper, Reporter):
    def __init__(self, client_timeout_seconds: int, protected: bool, storage_address: ObjectStorageAddress):
        self._client_timeout_seconds = client_timeout_seconds
        self._protected = protected
        self._storage_address = storage_address

        self._client_to_task_ids: OneToManyDict[ClientID, TaskID] = OneToManyDict()

        self._binder: Optional[AsyncBinder] = None
        self._binder_monitor: Optional[AsyncConnector] = None
        self._object_manager: Optional[ObjectManager] = None
        self._task_manager: Optional[TaskManager] = None
        self._worker_manager: Optional[WorkerManager] = None

        self._client_last_seen: Dict[ClientID, Tuple[float, ClientHeartbeat]] = dict()

    def register(
        self,
        binder: AsyncBinder,
        binder_monitor: AsyncConnector,
        object_manager: ObjectManager,
        task_manager: TaskManager,
        worker_manager: WorkerManager,
    ):
        self._binder = binder
        self._binder_monitor = binder_monitor
        self._object_manager = object_manager
        self._task_manager = task_manager
        self._worker_manager = worker_manager

    def get_client_task_ids(self, client_id: ClientID) -> Set[TaskID]:
        return self._client_to_task_ids.get_values(client_id)

    def has_client_id(self, client_id: ClientID) -> bool:
        return client_id in self._client_last_seen

    def get_client_id(self, task_id: TaskID) -> Optional[ClientID]:
        return self._client_to_task_ids.get_key(task_id)

    def on_task_begin(self, client_id: ClientID, task_id: TaskID):
        self._client_to_task_ids.add(client_id, task_id)

    def on_task_finish(self, task_id: TaskID) -> ClientID:
        return self._client_to_task_ids.remove_value(task_id)

    async def on_heartbeat(self, client_id: ClientID, info: ClientHeartbeat):
        await self._binder.send(
            client_id,
            ClientHeartbeatEcho.new_msg(object_storage_address=self._storage_address)
        )
        if client_id not in self._client_last_seen:
            logging.info(f"{client_id!r} connected")

        self._client_last_seen[client_id] = (time.time(), info)

    async def on_client_disconnect(self, client_id: ClientID, request: ClientDisconnect):
        if request.disconnect_type == ClientDisconnect.DisconnectType.Disconnect:
            await self.__on_client_disconnect(client_id)
            return

        if self._protected:
            logging.warning("cannot shutdown clusters as scheduler is running in protected mode")
            accepted = False
        else:
            logging.info(f"shutdown scheduler and all clusters as received signal from {client_id!r}")
            accepted = True

        await self._binder.send(client_id, ClientShutdownResponse.new_msg(accepted=accepted))

        if self._protected:
            return

        await self._worker_manager.on_client_shutdown(client_id)

        raise ClientShutdownException(f"received client shutdown from {client_id!r}, quitting")

    async def routine(self):
        await self.__routine_cleanup_clients()

    def get_status(self) -> ClientManagerStatus:
        return ClientManagerStatus.new_msg(
            {client: len(task_ids) for client, task_ids in self._client_to_task_ids.items()}
        )

    async def __routine_cleanup_clients(self):
        now = time.time()
        dead_clients = {
            client
            for client, (last_seen, info) in self._client_last_seen.items()
            if now - last_seen > self._client_timeout_seconds
        }

        for client in dead_clients:
            await self.__on_client_disconnect(client)

    async def __on_client_disconnect(self, client_id: ClientID):
        logging.info(f"{client_id!r} disconnected")
        if client_id in self._client_last_seen:
            self._client_last_seen.pop(client_id)

        await self.__cancel_tasks(client_id)
        self._object_manager.clean_client(client_id)

    async def __cancel_tasks(self, client_id: ClientID):
        if client_id not in self._client_to_task_ids.keys():
            return

        tasks = self._client_to_task_ids.get_values(client_id).copy()
        for task in tasks:
            await self._task_manager.on_task_cancel(client_id, TaskCancel.new_msg(task))
