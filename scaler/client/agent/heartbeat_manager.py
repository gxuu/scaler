import time
from concurrent.futures import Future
from typing import Optional

import psutil

from scaler.client.agent.mixins import HeartbeatManager, ObjectManager
from scaler.io.async_connector import AsyncConnector
from scaler.protocol.python.common import ObjectStorageAddress
from scaler.protocol.python.message import ClientHeartbeat, ClientHeartbeatEcho
from scaler.protocol.python.status import Resource
from scaler.utility.mixins import Looper


class ClientHeartbeatManager(Looper, HeartbeatManager):
    def __init__(self, death_timeout_seconds: int, storage_address_future: Future[ObjectStorageAddress]):
        self._death_timeout_seconds = death_timeout_seconds
        self._storage_address = storage_address_future

        self._process = psutil.Process()

        self._last_scheduler_contact = time.time()
        self._start_timestamp_ns = 0
        self._latency_us = 0
        self._connected = False

        self._connector_external: Optional[AsyncConnector] = None
        self._object_manager: Optional[ObjectManager] = None

    def register(self, connector_external: AsyncConnector):
        self._connector_external = connector_external

    async def send_heartbeat(self):
        await self._connector_external.send(
            ClientHeartbeat.new_msg(
                Resource.new_msg(int(self._process.cpu_percent() * 10), self._process.memory_info().rss),
                self._latency_us,
            )
        )

    async def on_heartbeat_echo(self, heartbeat: ClientHeartbeatEcho):
        if not self._connected:
            self._connected = True

        self._last_scheduler_contact = time.time()
        if self._start_timestamp_ns == 0:
            # not handling echo if we didn't send out heartbeat
            return

        self._latency_us = int(((time.time_ns() - self._start_timestamp_ns) / 2) // 1_000)
        self._start_timestamp_ns = 0

        if self._storage_address.done():
            return

        self._storage_address.set_result(heartbeat.object_storage_address())

    async def routine(self):
        if time.time() - self._last_scheduler_contact > self._death_timeout_seconds:
            raise TimeoutError(
                f"Timeout when connecting to scheduler {self._connector_external.address} "
                f"in {self._death_timeout_seconds} seconds"
            )

        if self._start_timestamp_ns != 0:
            # already sent heartbeat, expecting heartbeat echo, so not sending
            return

        await self.send_heartbeat()
        self._start_timestamp_ns = time.time_ns()

    def get_storage_address(self) -> ObjectStorageAddress:
        """Returns the object storage configuration, or block until it receives it."""
        return self._storage_address.result()
