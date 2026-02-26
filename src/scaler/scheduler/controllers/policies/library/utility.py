from scaler.scheduler.controllers.mixins import PolicyController
from scaler.scheduler.controllers.policies.mixins import ScalerPolicy
from scaler.scheduler.controllers.policies.types import PolicyEngineType


def create_policy(policy_engine_type: str, policy_content: str) -> ScalerPolicy:
    engine_type = PolicyEngineType(policy_engine_type)

    if engine_type == PolicyEngineType.SIMPLE:
        from scaler.scheduler.controllers.policies.simple_policy.simple_policy import SimplePolicy

        return SimplePolicy(policy_content)

    raise ValueError(f"Unknown policy_engine_type: {policy_engine_type}")


def create_policy_controller(policy_engine_type: str, policy_content: str) -> PolicyController:
    engine_type = PolicyEngineType(policy_engine_type)

    if engine_type == PolicyEngineType.SIMPLE:
        from scaler.scheduler.controllers.vanilla_policy_controller import VanillaPolicyController

        return VanillaPolicyController(policy_engine_type, policy_content)

    if engine_type == PolicyEngineType.WATERFALL_V1:
        from scaler.scheduler.controllers.policies.waterfall_v1.waterfall_v1_policy_controller import (
            WaterfallV1PolicyController,
        )

        return WaterfallV1PolicyController(policy_content)

    raise ValueError(f"Unknown policy type: {policy_engine_type}")
