from __future__ import annotations
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    mqtt_host: str = "core-mosquitto"
    mqtt_port: int = 1883
    mqtt_user: str = ""
    mqtt_pass: str = ""
    log_level: str = "info"
    http_port: int = 80
    discovery_port: int = 5000
    data_dir: str = "data"

    model_config = {"env_prefix": ""}


settings = Settings()
