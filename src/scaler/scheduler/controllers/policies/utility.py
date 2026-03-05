from scaler.scheduler.controllers.mixins import PolicyController


def create_policy_controller(policy_engine_type: str, policy_content: str) -> PolicyController:
    if policy_engine_type == "simple":
        from scaler.scheduler.controllers.vanilla_policy_controller import VanillaPolicyController

        return VanillaPolicyController(policy_content)

    raise ValueError(f"Unknown policy type: {policy_engine_type}")
