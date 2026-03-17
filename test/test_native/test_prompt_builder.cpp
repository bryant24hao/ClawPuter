// Native test for AI prompt construction with moisture context.
// Validates that moisture info goes into user message prefix, NOT system prompt.
//
// Run: cd ~/ClawPuter && pio test -e native
//
#include <ArduinoJson.h>
#include <cassert>
#include <cstdio>
#include <cstring>

// ─── Minimal reproduction of buildRequestDoc logic ───
// We extract the prompt-building logic so it can be tested without
// WiFiClient, M5Cardputer, or any ESP32 dependencies.

static const char* STATIC_SYSTEM_PROMPT =
    "You are a tiny pixel companion living inside a Cardputer device. "
    "Keep responses very short (1-2 sentences max) since the screen is tiny (240x135). "
    "Be friendly and playful. Use simple words. "
    "NEVER use emoji, markdown formatting, or special Unicode characters. Plain text only.";

// Build the hydration prefix that goes before the user message.
// Returns length written (0 if no prefix needed).
static int buildHydrationPrefix(int moisture, char* buf, int bufSize) {
    if (moisture > 3) return 0;  // fully hydrated, no prefix
    const char* tag;
    switch (moisture) {
        case 0: tag = "[you are extremely thirsty, beg for water] "; break;
        case 1: tag = "[you are very thirsty, mention needing water] "; break;
        case 2: tag = "[you are a bit thirsty] "; break;
        case 3: tag = "[you feel good] "; break;
        default: return 0;
    }
    int len = snprintf(buf, bufSize, "%s", tag);
    return (len >= bufSize) ? bufSize - 1 : len;
}

// Simulate buildRequestDoc with moisture context
static void buildTestDoc(const char* userMessage, int moisture, JsonDocument& doc) {
    doc["model"] = "openclaw";
    doc["stream"] = true;

    JsonArray messages = doc["messages"].to<JsonArray>();

    // System prompt — MUST remain static, no moisture info
    JsonObject sysMsg = messages.add<JsonObject>();
    sysMsg["role"] = "system";
    sysMsg["content"] = STATIC_SYSTEM_PROMPT;

    // User message — prepend hydration tag if needed
    JsonObject userMsg = messages.add<JsonObject>();
    userMsg["role"] = "user";

    char prefix[64];
    int prefixLen = buildHydrationPrefix(moisture, prefix, sizeof(prefix));
    if (prefixLen > 0) {
        char combined[256];
        snprintf(combined, sizeof(combined), "%s%s", prefix, userMessage);
        userMsg["content"] = combined;
    } else {
        userMsg["content"] = userMessage;
    }
}

// ─── Tests ───

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name()
#define RUN(name) do { \
    printf("  %-50s ", #name); \
    test_##name(); \
    printf("PASS\n"); \
    tests_passed++; \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_STR_CONTAINS(haystack, needle) do { \
    if (!strstr(haystack, needle)) { \
        printf("FAIL\n    %s:%d: \"%s\" not found in \"%s\"\n", __FILE__, __LINE__, needle, haystack); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_STR_NOT_CONTAINS(haystack, needle) do { \
    if (strstr(haystack, needle)) { \
        printf("FAIL\n    %s:%d: \"%s\" should NOT be in \"%s\"\n", __FILE__, __LINE__, needle, haystack); \
        tests_failed++; \
        return; \
    } \
} while(0)

// --- Test: system prompt never changes regardless of moisture ---

TEST(system_prompt_unchanged_at_moisture_0) {
    JsonDocument doc;
    buildTestDoc("hello", 0, doc);
    const char* sys = doc["messages"][0]["content"];
    ASSERT(strcmp(sys, STATIC_SYSTEM_PROMPT) == 0);
}

TEST(system_prompt_unchanged_at_moisture_4) {
    JsonDocument doc;
    buildTestDoc("hello", 4, doc);
    const char* sys = doc["messages"][0]["content"];
    ASSERT(strcmp(sys, STATIC_SYSTEM_PROMPT) == 0);
}

TEST(system_prompt_no_moisture_keywords) {
    for (int m = 0; m <= 4; m++) {
        JsonDocument doc;
        buildTestDoc("test", m, doc);
        const char* sys = doc["messages"][0]["content"];
        ASSERT_STR_NOT_CONTAINS(sys, "thirsty");
        ASSERT_STR_NOT_CONTAINS(sys, "hydrat");
        ASSERT_STR_NOT_CONTAINS(sys, "water");
        ASSERT_STR_NOT_CONTAINS(sys, "moisture");
    }
}

// --- Test: hydration prefix in user message ---

TEST(moisture_0_user_msg_has_beg_for_water) {
    JsonDocument doc;
    buildTestDoc("how are you", 0, doc);
    const char* user = doc["messages"][1]["content"];
    ASSERT_STR_CONTAINS(user, "extremely thirsty");
    ASSERT_STR_CONTAINS(user, "beg for water");
    ASSERT_STR_CONTAINS(user, "how are you");  // original message preserved
}

TEST(moisture_1_user_msg_has_thirsty) {
    JsonDocument doc;
    buildTestDoc("hello", 1, doc);
    const char* user = doc["messages"][1]["content"];
    ASSERT_STR_CONTAINS(user, "very thirsty");
    ASSERT_STR_CONTAINS(user, "hello");
}

TEST(moisture_2_user_msg_has_bit_thirsty) {
    JsonDocument doc;
    buildTestDoc("hi", 2, doc);
    const char* user = doc["messages"][1]["content"];
    ASSERT_STR_CONTAINS(user, "bit thirsty");
    ASSERT_STR_CONTAINS(user, "hi");
}

TEST(moisture_3_user_msg_has_feel_good) {
    JsonDocument doc;
    buildTestDoc("what's up", 3, doc);
    const char* user = doc["messages"][1]["content"];
    ASSERT_STR_CONTAINS(user, "feel good");
    ASSERT_STR_CONTAINS(user, "what's up");
}

TEST(moisture_4_user_msg_no_prefix) {
    JsonDocument doc;
    buildTestDoc("hello there", 4, doc);
    const char* user = doc["messages"][1]["content"];
    // No prefix at full hydration — message should be exactly the original
    ASSERT(strcmp(user, "hello there") == 0);
}

// --- Test: prefix doesn't corrupt message ---

TEST(prefix_preserves_special_chars) {
    JsonDocument doc;
    buildTestDoc("what's 2+2?", 1, doc);
    const char* user = doc["messages"][1]["content"];
    ASSERT_STR_CONTAINS(user, "what's 2+2?");
}

TEST(prefix_preserves_chinese) {
    JsonDocument doc;
    buildTestDoc("你好", 1, doc);
    const char* user = doc["messages"][1]["content"];
    ASSERT_STR_CONTAINS(user, "你好");
}

// --- Test: JSON structure is valid ---

TEST(json_has_correct_structure) {
    JsonDocument doc;
    buildTestDoc("test", 2, doc);
    ASSERT(doc["model"] == "openclaw");
    ASSERT(doc["stream"] == true);
    JsonArray msgs = doc["messages"];
    ASSERT(msgs.size() == 2);  // system + user
    ASSERT(strcmp(msgs[0]["role"], "system") == 0);
    ASSERT(strcmp(msgs[1]["role"], "user") == 0);
}

// --- Test: buildHydrationPrefix edge cases ---

TEST(prefix_buffer_too_small) {
    char buf[10];
    int len = buildHydrationPrefix(0, buf, sizeof(buf));
    // Should truncate safely, not overflow
    ASSERT(len < 10);
    ASSERT(strlen(buf) < 10);
}

TEST(prefix_returns_zero_for_full_hydration) {
    char buf[64];
    int len = buildHydrationPrefix(4, buf, sizeof(buf));
    ASSERT(len == 0);
}

TEST(prefix_returns_zero_for_invalid_moisture) {
    char buf[64];
    int len = buildHydrationPrefix(5, buf, sizeof(buf));
    ASSERT(len == 0);
    len = buildHydrationPrefix(-1, buf, sizeof(buf));
    ASSERT(len == 0);
}

// ═══════════════════════════════════════════════════════════════
// Thinking Model Compatibility Tests
// ═══════════════════════════════════════════════════════════════

// Reproduce the extractContent lambda from ai_client.cpp
static int extractContent(const char* json, char* outBuf, int outSize) {
    const char* key = strstr(json, "\"content\":\"");
    if (!key) return 0;
    const char* p = key + 11;
    int i = 0;
    while (*p && *p != '"' && i < outSize - 1) {
        if (*p == '\\' && p[1]) {
            switch (p[1]) {
                case '"':  outBuf[i++] = '"';  p += 2; break;
                case '\\': outBuf[i++] = '\\'; p += 2; break;
                case 'n':  outBuf[i++] = '\n'; p += 2; break;
                case '/':  outBuf[i++] = '/';  p += 2; break;
                default:   p += 2; break;
            }
        } else {
            outBuf[i++] = *p++;
        }
    }
    outBuf[i] = '\0';
    return i;
}

// Simulate processSSELine logic: returns true if thinking, false if real content
static bool isThinkingChunk(const char* sseData) {
    char contentBuf[128];
    int clen = extractContent(sseData, contentBuf, sizeof(contentBuf));
    if (clen >= 6 && memcmp(contentBuf, "think\n", 6) == 0) return true;
    return false;
}

// Check if content contains gateway fallback injection
static bool isGatewayFallback(const char* sseData) {
    char contentBuf[128];
    int clen = extractContent(sseData, contentBuf, sizeof(contentBuf));
    if (clen > 0) {
        if (strstr(contentBuf, "Continue where you left off")) return true;
        if (strstr(contentBuf, "previous model attempt")) return true;
    }
    return false;
}

// --- Thinking filter tests ---

TEST(thinking_chunk_detected) {
    // Real SSE data from gateway with thinking
    const char* sse = "{\"choices\":[{\"delta\":{\"content\":\"think\\nThe user is asking...\"}}]}";
    ASSERT(isThinkingChunk(sse));
}

TEST(normal_content_not_filtered) {
    const char* sse = "{\"choices\":[{\"delta\":{\"content\":\"Hi! I am your companion\"}}]}";
    ASSERT(!isThinkingChunk(sse));
}

TEST(content_starting_with_think_word_not_filtered) {
    // "Think of me as..." should NOT be filtered (no \n after think)
    const char* sse = "{\"choices\":[{\"delta\":{\"content\":\"Think of me as a friend\"}}]}";
    ASSERT(!isThinkingChunk(sse));
}

TEST(empty_content_not_filtered) {
    const char* sse = "{\"choices\":[{\"delta\":{\"content\":\"\"}}]}";
    ASSERT(!isThinkingChunk(sse));
}

TEST(no_content_field_not_filtered) {
    const char* sse = "{\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ASSERT(!isThinkingChunk(sse));
}

TEST(thinking_with_long_content_detected) {
    // Thinking content that exceeds contentBuf (128 bytes) — prefix still detectable
    char sse[512];
    snprintf(sse, sizeof(sse),
        "{\"choices\":[{\"delta\":{\"content\":\"think\\n%s\"}}]}",
        "The user is asking me to continue where I left off because the previous model attempt "
        "failed or timed out and I need to check the session context to understand what happened");
    ASSERT(isThinkingChunk(sse));
}

// --- Gateway fallback detection tests ---

TEST(fallback_continue_detected) {
    const char* sse = "{\"choices\":[{\"delta\":{\"content\":\"Continue where you left off\"}}]}";
    ASSERT(isGatewayFallback(sse));
}

TEST(fallback_previous_model_detected) {
    const char* sse = "{\"choices\":[{\"delta\":{\"content\":\"The previous model attempt failed\"}}]}";
    ASSERT(isGatewayFallback(sse));
}

TEST(normal_content_not_fallback) {
    const char* sse = "{\"choices\":[{\"delta\":{\"content\":\"Hi! How can I help you?\"}}]}";
    ASSERT(!isGatewayFallback(sse));
}

TEST(fallback_not_in_thinking_chunk) {
    // Thinking chunk that mentions "Continue" — isThinkingChunk catches it first
    const char* sse = "{\"choices\":[{\"delta\":{\"content\":\"think\\nContinue where you left off\"}}]}";
    // This IS a thinking chunk (caught by thinking filter, never reaches fallback check)
    ASSERT(isThinkingChunk(sse));
}

// --- extractContent edge cases ---

TEST(extract_does_not_match_reasoning_content) {
    // "reasoning_content" should NOT match — the " before content doesn't match _
    const char* sse = "{\"choices\":[{\"delta\":{\"reasoning_content\":\"thinking text\",\"content\":null}}]}";
    char buf[128];
    int len = extractContent(sse, buf, sizeof(buf));
    // "content":null has no opening quote after colon, should not match "content":"
    ASSERT(len == 0);
}

TEST(extract_handles_escaped_quotes) {
    const char* sse = "{\"choices\":[{\"delta\":{\"content\":\"He said \\\"hello\\\"\"}}]}";
    char buf[128];
    int len = extractContent(sse, buf, sizeof(buf));
    ASSERT(len > 0);
    ASSERT_STR_CONTAINS(buf, "He said \"hello\"");
}

TEST(extract_handles_newlines) {
    const char* sse = "{\"choices\":[{\"delta\":{\"content\":\"line1\\nline2\"}}]}";
    char buf[128];
    int len = extractContent(sse, buf, sizeof(buf));
    ASSERT(len > 0);
    ASSERT_STR_CONTAINS(buf, "line1\nline2");
}

// --- Timeout logic tests (pure arithmetic, no millis()) ---

TEST(overflow_safe_timeout_normal) {
    // Simulate: start=1000, lastActivity=5000, now=10000
    unsigned long start = 1000, lastAct = 5000, now = 10000;
    bool idle = (now - lastAct > 30000);
    bool maxed = (now - start > 120000);
    ASSERT(!idle);   // 5s idle, under 30s limit
    ASSERT(!maxed);  // 9s total, under 120s limit
}

TEST(overflow_safe_timeout_idle_exceeded) {
    unsigned long start = 1000, lastAct = 5000, now = 40000;
    bool idle = (now - lastAct > 30000);
    ASSERT(idle);  // 35s idle, over 30s limit
}

TEST(overflow_safe_timeout_max_exceeded) {
    unsigned long start = 1000, lastAct = 120500, now = 121500;
    bool idle = (now - lastAct > 30000);
    bool maxed = (now - start > 120000);
    ASSERT(!idle);  // 1s idle
    ASSERT(maxed);  // 120.5s total, over 120s limit
}

TEST(overflow_safe_timeout_millis_wrap) {
    // Simulate millis() overflow: start near ULONG_MAX
    unsigned long start = 0xFFFFFF00UL;
    unsigned long lastAct = 0xFFFFFF00UL + 10000;  // wraps
    unsigned long now = 0xFFFFFF00UL + 15000;       // wraps
    // Subtraction handles overflow correctly in unsigned arithmetic
    bool idle = (now - lastAct > 30000);
    bool maxed = (now - start > 120000);
    ASSERT(!idle);   // 5s idle
    ASSERT(!maxed);  // 15s total
}

// ─── Main ───

int main() {
    printf("\n=== Prompt Builder Tests ===\n\n");

    RUN(system_prompt_unchanged_at_moisture_0);
    RUN(system_prompt_unchanged_at_moisture_4);
    RUN(system_prompt_no_moisture_keywords);
    RUN(moisture_0_user_msg_has_beg_for_water);
    RUN(moisture_1_user_msg_has_thirsty);
    RUN(moisture_2_user_msg_has_bit_thirsty);
    RUN(moisture_3_user_msg_has_feel_good);
    RUN(moisture_4_user_msg_no_prefix);
    RUN(prefix_preserves_special_chars);
    RUN(prefix_preserves_chinese);
    RUN(json_has_correct_structure);
    RUN(prefix_buffer_too_small);
    RUN(prefix_returns_zero_for_full_hydration);
    RUN(prefix_returns_zero_for_invalid_moisture);

    printf("\n=== Thinking Model Compatibility Tests ===\n\n");

    RUN(thinking_chunk_detected);
    RUN(normal_content_not_filtered);
    RUN(content_starting_with_think_word_not_filtered);
    RUN(empty_content_not_filtered);
    RUN(no_content_field_not_filtered);
    RUN(thinking_with_long_content_detected);
    RUN(fallback_continue_detected);
    RUN(fallback_previous_model_detected);
    RUN(normal_content_not_fallback);
    RUN(fallback_not_in_thinking_chunk);
    RUN(extract_does_not_match_reasoning_content);
    RUN(extract_handles_escaped_quotes);
    RUN(extract_handles_newlines);
    RUN(overflow_safe_timeout_normal);
    RUN(overflow_safe_timeout_idle_exceeded);
    RUN(overflow_safe_timeout_max_exceeded);
    RUN(overflow_safe_timeout_millis_wrap);

    printf("\n=== Results: %d passed, %d failed ===\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
