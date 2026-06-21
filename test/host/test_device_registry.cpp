#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <setjmp.h>
#include <cstdint>

// Include production header
#include "app_device_registry.h"
#include "esp_timer.h"
#include "nvs_flash.h"

// --- Minimal test framework ---
static int s_tests_passed = 0;
static int s_tests_failed = 0;
static jmp_buf s_assert_jmp;

#define TEST(name) \
    static void test_##name(void); \
    struct Register_##name { \
        Register_##name() { \
            if (s_first) { \
                s_first = false; \
                fprintf(stdout, "\nRunning tests for %s...\n\n", s_suite_name); \
            } \
            fprintf(stdout, "  TEST: %-50s ", #name); \
            fflush(stdout); \
            if (setjmp(s_assert_jmp) == 0) { \
                test_##name(); \
                fprintf(stdout, "PASSED\n"); \
                s_tests_passed++; \
            } else { \
                s_tests_failed++; \
            } \
        } \
    }; \
    static Register_##name s_reg_##name; \
    static void test_##name(void)

static const char *s_suite_name = "";
static bool s_first = true;

#define SUITE(name) \
    static void init_suite(void); \
    namespace { \
        struct SuiteInit { \
            SuiteInit() { \
                s_suite_name = name; \
                s_first = true; \
                init_suite(); \
            } \
        } s_suite_init; \
    }

#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "\n  FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
                longjmp(s_assert_jmp, 1); \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual) do { \
    auto _e = (expected); auto _a = (actual); \
    if (_e != _a) { \
        fprintf(stderr, "\n  FAILED at %s:%d: expected '%s' (%lld), got '%s' (%lld)\n", \
                __FILE__, __LINE__, #expected, (long long)_e, #actual, (long long)_a); \
                longjmp(s_assert_jmp, 1); \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual) do { \
    const char *_e = (expected); const char *_a = (actual); \
    if (!_a) { \
        fprintf(stderr, "\n  FAILED at %s:%d: expected \"%s\", got NULL\n", \
                __FILE__, __LINE__, _e); \
        longjmp(s_assert_jmp, 1); \
    } \
    if (strcmp(_e, _a) != 0) { \
        fprintf(stderr, "\n  FAILED at %s:%d: expected \"%s\", got \"%s\"\n", \
                __FILE__, __LINE__, _e, _a); \
                longjmp(s_assert_jmp, 1); \
        } \
    } while(0)

#define ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        fprintf(stderr, "\n  FAILED at %s:%d: expected NULL, got non-NULL\n", \
                __FILE__, __LINE__); \
                longjmp(s_assert_jmp, 1); \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        fprintf(stderr, "\n  FAILED at %s:%d: expected non-NULL, got NULL\n", \
                __FILE__, __LINE__); \
                longjmp(s_assert_jmp, 1); \
        } \
    } while(0)

#define ASSERT_EQ_INT(expected, actual) ASSERT_EQ(expected, actual)

// --- Test suite ---
SUITE("device_registry")

static void init_suite(void)
{
    mock_nvs_clear();
    mock_esp_timer_set_time(1000000);
    device_registry_init();
}

// Helpers
static int count_registered(void)
{
    int c = 0;
    device_registry_get_all(&c);
    return c;
}

// ===== Type conversion tests =====

TEST(type_from_string_onoff)
{
    ASSERT_EQ(DEVICE_TYPE_ON_OFF, device_type_from_string("onoff"));
}

TEST(type_from_string_dimmable)
{
    ASSERT_EQ(DEVICE_TYPE_DIMMABLE, device_type_from_string("dimmable"));
}

TEST(type_from_string_temperature)
{
    ASSERT_EQ(DEVICE_TYPE_TEMPERATURE_SENSOR, device_type_from_string("temperature"));
}

TEST(type_from_string_humidity)
{
    ASSERT_EQ(DEVICE_TYPE_HUMIDITY_SENSOR, device_type_from_string("humidity"));
}

TEST(type_from_string_contact)
{
    ASSERT_EQ(DEVICE_TYPE_CONTACT_SENSOR, device_type_from_string("contact"));
}

TEST(type_from_string_occupancy)
{
    ASSERT_EQ(DEVICE_TYPE_OCCUPANCY_SENSOR, device_type_from_string("occupancy"));
}

TEST(type_from_string_light_sensor)
{
    ASSERT_EQ(DEVICE_TYPE_LIGHT_SENSOR, device_type_from_string("light_sensor"));
}

TEST(type_from_string_tanque)
{
    ASSERT_EQ(DEVICE_TYPE_TANQUE, device_type_from_string("tanque"));
}

TEST(type_from_string_gas)
{
    ASSERT_EQ(DEVICE_TYPE_GAS_SENSOR, device_type_from_string("gas"));
}

TEST(type_from_string_rain)
{
    ASSERT_EQ(DEVICE_TYPE_RAIN_SENSOR, device_type_from_string("rain"));
}

TEST(type_from_string_electricity)
{
    ASSERT_EQ(DEVICE_TYPE_ELECTRICITY, device_type_from_string("electricity"));
}

TEST(type_from_string_unknown)
{
    ASSERT_EQ(DEVICE_TYPE_UNKNOWN, device_type_from_string("invalid_type"));
    ASSERT_EQ(DEVICE_TYPE_UNKNOWN, device_type_from_string(""));
}

TEST(type_to_string_onoff)
{
    ASSERT_STR_EQ("onoff", device_type_to_string(DEVICE_TYPE_ON_OFF));
}

TEST(type_to_string_unknown)
{
    ASSERT_STR_EQ("unknown", device_type_to_string(DEVICE_TYPE_UNKNOWN));
}

TEST(type_roundtrip)
{
    device_type_t types[] = {
        DEVICE_TYPE_ON_OFF, DEVICE_TYPE_DIMMABLE,
        DEVICE_TYPE_TEMPERATURE_SENSOR, DEVICE_TYPE_HUMIDITY_SENSOR,
        DEVICE_TYPE_CONTACT_SENSOR, DEVICE_TYPE_OCCUPANCY_SENSOR,
        DEVICE_TYPE_LIGHT_SENSOR, DEVICE_TYPE_TANQUE,
        DEVICE_TYPE_GAS_SENSOR, DEVICE_TYPE_RAIN_SENSOR,
        DEVICE_TYPE_ELECTRICITY
    };
    const char *names[] = {
        "onoff", "dimmable", "temperature", "humidity",
        "contact", "occupancy", "light_sensor", "tanque",
        "gas", "rain", "electricity"
    };
    for (int i = 0; i < 11; i++) {
        ASSERT_EQ(types[i], device_type_from_string(names[i]));
    }
}

// ===== Registration tests =====

TEST(register_device)
{
    int slot = device_registry_register("test1", DEVICE_TYPE_ON_OFF, "Test 1", "192.168.1.10");
    ASSERT(slot >= 0);
}

TEST(register_and_find)
{
    int slot = device_registry_register("find_me", DEVICE_TYPE_TEMPERATURE_SENSOR, "Sensor", "10.0.0.1");
    (void)slot;
    bridged_device_t *dev = device_registry_get_by_id("find_me");
    ASSERT_NOT_NULL(dev);
    ASSERT_STR_EQ("find_me", dev->id);
    ASSERT_STR_EQ("Sensor", dev->name);
    ASSERT_STR_EQ("10.0.0.1", dev->ip);
    ASSERT_EQ(DEVICE_TYPE_TEMPERATURE_SENSOR, dev->type);
    ASSERT(dev->registered);
    ASSERT(dev->online);
}

TEST(register_duplicate_id)
{
    int s1 = device_registry_register("dup", DEVICE_TYPE_ON_OFF, "First", "1.1.1.1");
    int s2 = device_registry_register("dup", DEVICE_TYPE_TEMPERATURE_SENSOR, "Second", "2.2.2.2");
    ASSERT_EQ(s1, s2);
    bridged_device_t *dev = device_registry_get_by_id("dup");
    ASSERT_NOT_NULL(dev);
    ASSERT_STR_EQ("Second", dev->name);
    ASSERT_STR_EQ("2.2.2.2", dev->ip);
    ASSERT_EQ(DEVICE_TYPE_TEMPERATURE_SENSOR, dev->type);
}

TEST(register_multiple_devices)
{
    int start = count_registered();
    char id[32];
    for (int i = 0; i < 5; i++) {
        snprintf(id, sizeof(id), "dev_%d", i);
        int slot = device_registry_register(id, DEVICE_TYPE_ON_OFF, id, "10.0.0.1");
        ASSERT(slot >= 0);
    }
    ASSERT_EQ(start + 5, count_registered());
    for (int i = 0; i < 5; i++) {
        snprintf(id, sizeof(id), "dev_%d", i);
        ASSERT_NOT_NULL(device_registry_get_by_id(id));
    }
}

TEST(register_device_increments_count)
{
    int before = count_registered();
    device_registry_register("count_test", DEVICE_TYPE_TANQUE, "Tank", "10.0.0.5");
    ASSERT_EQ(before + 1, count_registered());
}

TEST(register_without_name_fallback_to_id)
{
    int slot = device_registry_register("no_name_dev", DEVICE_TYPE_GAS_SENSOR, NULL, "10.0.0.1");
    (void)slot;
    bridged_device_t *dev = device_registry_get_by_id("no_name_dev");
    ASSERT_NOT_NULL(dev);
    ASSERT_STR_EQ("no_name_dev", dev->name);
}

// ===== Lookup tests =====

TEST(get_by_id_returns_null_for_unknown)
{
    ASSERT_NULL(device_registry_get_by_id("nonexistent"));
}

TEST(get_by_id_returns_null_after_remove)
{
    device_registry_register("temp_dev", DEVICE_TYPE_CONTACT_SENSOR, "Temp", "1.1.1.1");
    device_registry_remove_device("temp_dev");
    ASSERT_NULL(device_registry_get_by_id("temp_dev"));
}

// ===== Removal tests =====

TEST(remove_existing_device)
{
    device_registry_register("remove_me", DEVICE_TYPE_ON_OFF, "Remove", "1.1.1.1");
    esp_err_t err = device_registry_remove_device("remove_me");
    ASSERT_EQ(ESP_OK, err);
}

TEST(remove_nonexistent_device)
{
    esp_err_t err = device_registry_remove_device("i_dont_exist");
    ASSERT_EQ(ESP_ERR_NOT_FOUND, err);
}

TEST(remove_decrements_count)
{
    device_registry_register("count1", DEVICE_TYPE_ON_OFF, "C1", "1.1.1.1");
    device_registry_register("count2", DEVICE_TYPE_ON_OFF, "C2", "1.1.1.2");
    int before = count_registered();
    device_registry_remove_device("count1");
    ASSERT_EQ(before - 1, count_registered());
}

// ===== State tests =====

TEST(update_state_key_value)
{
    device_registry_register("state_dev", DEVICE_TYPE_ON_OFF, "State", "1.1.1.1");
    esp_err_t err = device_registry_update_state("state_dev", "power", "on");
    ASSERT_EQ(ESP_OK, err);
}

TEST(update_state_nonexistent_device)
{
    esp_err_t err = device_registry_update_state("no_dev", "power", "on");
    ASSERT_EQ(ESP_ERR_NOT_FOUND, err);
}

TEST(update_state_multiple_keys)
{
    device_registry_register("multi_state", DEVICE_TYPE_ON_OFF, "Multi", "1.1.1.1");
    device_registry_update_state("multi_state", "power", "on");
    device_registry_update_state("multi_state", "brightness", "75");
    char json[64];
    ASSERT_EQ(ESP_OK, device_registry_get_state_json("multi_state", json, sizeof(json)));
    ASSERT(strstr(json, "\"power\":\"on\"") != NULL);
    ASSERT(strstr(json, "\"brightness\":75") != NULL);
}

TEST(update_state_replaces_key)
{
    device_registry_register("replace_state", DEVICE_TYPE_DIMMABLE, "Replace", "1.1.1.1");
    device_registry_update_state("replace_state", "power", "on");
    device_registry_update_state("replace_state", "power", "off");
    char json[64];
    ASSERT_EQ(ESP_OK, device_registry_get_state_json("replace_state", json, sizeof(json)));
    ASSERT(strstr(json, "\"power\":\"off\"") != NULL);
    ASSERT(strstr(json, "\"power\":\"on\"") == NULL);
}

TEST(update_state_marks_device_online)
{
    device_registry_register("online_state", DEVICE_TYPE_ON_OFF, "Online", "1.1.1.1");
    device_registry_update_state("online_state", "power", "on");
    bridged_device_t *dev = device_registry_get_by_id("online_state");
    ASSERT_NOT_NULL(dev);
    ASSERT(dev->online);
}

TEST(get_state_json_empty)
{
    device_registry_register("empty_state", DEVICE_TYPE_ON_OFF, "Empty", "1.1.1.1");
    char json[64];
    ASSERT_EQ(ESP_OK, device_registry_get_state_json("empty_state", json, sizeof(json)));
    ASSERT_STR_EQ("{}", json);
}

TEST(get_state_json_unknown_device)
{
    char json[64];
    esp_err_t err = device_registry_get_state_json("ghost", json, sizeof(json));
    ASSERT(err == ESP_ERR_NOT_FOUND);
}

TEST(get_state_json_boolean)
{
    device_registry_register("bool_state", DEVICE_TYPE_ON_OFF, "Bool", "1.1.1.1");
    device_registry_update_state("bool_state", "power", "true");
    char json[64];
    ASSERT_EQ(ESP_OK, device_registry_get_state_json("bool_state", json, sizeof(json)));
    ASSERT(strstr(json, "\"power\":true") != NULL);
}

TEST(get_state_json_number)
{
    device_registry_register("num_state", DEVICE_TYPE_TEMPERATURE_SENSOR, "Num", "1.1.1.1");
    device_registry_update_state("num_state", "temperature", "25.5");
    char json[64];
    ASSERT_EQ(ESP_OK, device_registry_get_state_json("num_state", json, sizeof(json)));
    ASSERT(strstr(json, "\"temperature\":25.5") != NULL);
}

TEST(get_state_json_negative_number)
{
    device_registry_register("neg_state", DEVICE_TYPE_TEMPERATURE_SENSOR, "Neg", "1.1.1.1");
    device_registry_update_state("neg_state", "temp", "-5");
    char json[64];
    ASSERT_EQ(ESP_OK, device_registry_get_state_json("neg_state", json, sizeof(json)));
    ASSERT(strstr(json, "\"temp\":-5") != NULL);
}

// ===== Command queue tests =====

TEST(add_command_to_device)
{
    device_registry_register("cmd_dev", DEVICE_TYPE_ON_OFF, "Cmd", "1.1.1.1");
    esp_err_t err = device_registry_add_command("cmd_dev", "cluster1", "toggle", NULL);
    ASSERT_EQ(ESP_OK, err);
}

TEST(add_command_to_nonexistent_device)
{
    esp_err_t err = device_registry_add_command("no_dev", "c", "toggle", NULL);
    ASSERT_EQ(ESP_ERR_NOT_FOUND, err);
}

TEST(get_commands_returns_queued)
{
    device_registry_register("cmd_queue", DEVICE_TYPE_ON_OFF, "Queue", "1.1.1.1");
    device_registry_add_command("cmd_queue", "cluster1", "toggle", NULL);
    device_registry_add_command("cmd_queue", "cluster2", "set", "42");

    pending_command_t cmds[MAX_PENDING_COMMANDS];
    int count = device_registry_get_commands("cmd_queue", cmds, MAX_PENDING_COMMANDS);
    ASSERT_EQ(2, count);
    ASSERT_STR_EQ("cluster1", cmds[0].cluster);
    ASSERT_STR_EQ("toggle", cmds[0].command);
    ASSERT_STR_EQ("cluster2", cmds[1].cluster);
    ASSERT_STR_EQ("set", cmds[1].command);
    ASSERT_STR_EQ("42", cmds[1].data);
}

TEST(get_commands_clears_queue)
{
    device_registry_register("cmd_clear", DEVICE_TYPE_ON_OFF, "Clear", "1.1.1.1");
    device_registry_add_command("cmd_clear", "c", "toggle", NULL);

    pending_command_t cmds[MAX_PENDING_COMMANDS];
    int count = device_registry_get_commands("cmd_clear", cmds, MAX_PENDING_COMMANDS);
    ASSERT_EQ(1, count);

    count = device_registry_get_commands("cmd_clear", cmds, MAX_PENDING_COMMANDS);
    ASSERT_EQ(0, count);
}

TEST(get_commands_unknown_device)
{
    pending_command_t cmds[MAX_PENDING_COMMANDS];
    int count = device_registry_get_commands("ghost", cmds, MAX_PENDING_COMMANDS);
    ASSERT_EQ(0, count);
}

TEST(get_commands_with_data)
{
    device_registry_register("cmd_data", DEVICE_TYPE_ON_OFF, "Data", "1.1.1.1");
    device_registry_add_command("cmd_data", "level", "set", "{\"value\":80}");
    pending_command_t cmds[MAX_PENDING_COMMANDS];
    int count = device_registry_get_commands("cmd_data", cmds, MAX_PENDING_COMMANDS);
    ASSERT_EQ(1, count);
    ASSERT_STR_EQ("{\"value\":80}", cmds[0].data);
}

// ===== Mark seen tests =====

TEST(mark_seen_updates_timestamp)
{
    device_registry_register("seen_dev", DEVICE_TYPE_ON_OFF, "Seen", "1.1.1.1");
    bridged_device_t *dev = device_registry_get_by_id("seen_dev");
    int64_t before = dev->last_seen_us;

    mock_esp_timer_advance(5000000); // 5 seconds
    device_registry_mark_seen("seen_dev");

    ASSERT(dev->last_seen_us > before);
}

TEST(mark_seen_unknown_device)
{
    device_registry_mark_seen("ghost");
}

// ===== Get all tests =====

TEST(get_all_returns_registered_count)
{
    int count = 0;
    bridged_device_t *devices = device_registry_get_all(&count);
    ASSERT_NOT_NULL(devices);
    ASSERT(count >= 0);
}

TEST(get_all_null_count)
{
    bridged_device_t *devices = device_registry_get_all(NULL);
    ASSERT_NOT_NULL(devices);
}

// ===== Rmaker handle tests =====

TEST(set_and_get_rmaker_handle)
{
    device_registry_register("rmaker_dev", DEVICE_TYPE_ON_OFF, "RMaker", "1.1.1.1");
    void *handle = (void *)(uintptr_t)0xDEAD;
    device_registry_set_rmaker_handle("rmaker_dev", handle);
    void *got = device_registry_get_rmaker_handle("rmaker_dev");
    ASSERT_EQ(handle, got);
}

TEST(get_rmaker_handle_unset)
{
    device_registry_register("rmaker_null", DEVICE_TYPE_ON_OFF, "Null", "1.1.1.1");
    void *got = device_registry_get_rmaker_handle("rmaker_null");
    ASSERT_NULL(got);
}

// ===== Main =====
int main(void)
{
    fprintf(stdout, "\n===== Device Registry Tests =====\n");
    fprintf(stdout, "Tests passed: %d\n", s_tests_passed);
    fprintf(stdout, "Tests failed: %d\n", s_tests_failed);
    return s_tests_failed > 0 ? 1 : 0;
}
