# Pre-build: custom_app_name / custom_app_version z platformio.ini
# -> makra APP_NAME / APP_VERSION widoczne w firmware (np. odpowiedz na DISCOVER).
Import("env")

name = env.GetProjectOption("custom_app_name")
version = env.GetProjectOption("custom_app_version")

# esp_app_desc_t ma pola char[32] z koncowym zerem
for label, value in (("custom_app_name", name), ("custom_app_version", version)):
    if len(value.encode()) > 31:
        raise SystemExit(f"{label} za dlugie (max 31 bajtow): {value!r}")

env.Append(CPPDEFINES=[
    ("APP_NAME", env.StringifyMacro(name)),
    ("APP_VERSION", env.StringifyMacro(version)),
])
