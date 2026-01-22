from typing import Tuple

from scaler.scheduler.controllers.policies.load.mixins import ScalerPolicy


def create_scaler_policy(policy_type: str, policy_strategy: str, webhook_urls: Tuple[str, ...]) -> ScalerPolicy:
    parts = {
        k.strip(): v.strip() for item in policy_strategy.split(";") if "=" in item for k, v in [item.split("=", 1)]
    }

    if policy_type == "legacy":
        from scaler.scheduler.controllers.policies.load.legacy_policy import LegacyPolicy

        return LegacyPolicy(parts, webhook_urls)
    raise ValueError("Unknown policy type")
