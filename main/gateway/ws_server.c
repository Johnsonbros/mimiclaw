#include "ws_server.h"
#include "mimi_config.h"
#include "bus/message_bus.h"

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"

static const char *TAG = "ws";

static httpd_handle_t s_server = NULL;

/* Simple client tracking */
typedef struct {
    int fd;
    char chat_id[32];
    bool active;
} ws_client_t;

static ws_client_t s_clients[MIMI_WS_MAX_CLIENTS];

static ws_client_t *find_client_by_fd(int fd)
{
    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && s_clients[i].fd == fd) {
            return &s_clients[i];
        }
    }
    return NULL;
}

static ws_client_t *find_client_by_chat_id(const char *chat_id)
{
    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && strcmp(s_clients[i].chat_id, chat_id) == 0) {
            return &s_clients[i];
        }
    }
    return NULL;
}

static ws_client_t *add_client(int fd)
{
    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        if (!s_clients[i].active) {
            s_clients[i].fd = fd;
            snprintf(s_clients[i].chat_id, sizeof(s_clients[i].chat_id), "ws_%d", fd);
            s_clients[i].active = true;
            ESP_LOGI(TAG, "Client connected: %s (fd=%d)", s_clients[i].chat_id, fd);
            return &s_clients[i];
        }
    }
    ESP_LOGW(TAG, "Max clients reached, rejecting fd=%d", fd);
    return NULL;
}

static void remove_client(int fd)
{
    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && s_clients[i].fd == fd) {
            ESP_LOGI(TAG, "Client disconnected: %s", s_clients[i].chat_id);
            s_clients[i].active = false;
            return;
        }
    }
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        /* WebSocket handshake — register client */
        int fd = httpd_req_to_sockfd(req);
        add_client(fd);
        return ESP_OK;
    }

    /* Receive WebSocket frame */
    httpd_ws_frame_t ws_pkt = {0};
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    /* Get frame length */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    if (ws_pkt.len == 0) return ESP_OK;

    ws_pkt.payload = calloc(1, ws_pkt.len + 1);
    if (!ws_pkt.payload) return ESP_ERR_NO_MEM;

    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        free(ws_pkt.payload);
        return ret;
    }

    int fd = httpd_req_to_sockfd(req);
    ws_client_t *client = find_client_by_fd(fd);

    /* Parse JSON message */
    cJSON *root = cJSON_Parse((char *)ws_pkt.payload);
    free(ws_pkt.payload);

    if (!root) {
        ESP_LOGW(TAG, "Invalid JSON from fd=%d", fd);
        return ESP_OK;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    cJSON *content = cJSON_GetObjectItem(root, "content");

    if (type && cJSON_IsString(type) && strcmp(type->valuestring, "message") == 0
        && content && cJSON_IsString(content)) {

        /* Determine chat_id */
        const char *chat_id = client ? client->chat_id : "ws_unknown";
        cJSON *cid = cJSON_GetObjectItem(root, "chat_id");
        if (cid && cJSON_IsString(cid)) {
            chat_id = cid->valuestring;
            /* Update client's chat_id if provided */
            if (client) {
                strncpy(client->chat_id, chat_id, sizeof(client->chat_id) - 1);
            }
        }

        ESP_LOGI(TAG, "WS message from %s: %.40s...", chat_id, content->valuestring);

        /* Push to inbound bus */
        mimi_msg_t msg = {0};
        strncpy(msg.channel, MIMI_CHAN_WEBSOCKET, sizeof(msg.channel) - 1);
        strncpy(msg.chat_id, chat_id, sizeof(msg.chat_id) - 1);
        msg.content = strdup(content->valuestring);
        if (msg.content) {
            message_bus_push_inbound(&msg);
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

/* ── Dashboard HTML ─────────────────────────── */
static const char DASHBOARD_HTML[] =
"<!DOCTYPE html>"
"<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>DOT - AiSync Services</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{font-family:-apple-system,system-ui,sans-serif;background:#0a0e17;color:#e0e0e0;height:100vh;display:flex;flex-direction:column}"
"header{background:#111827;padding:16px 20px;display:flex;align-items:center;gap:12px;border-bottom:1px solid #1f2937}"
"header .dot{width:10px;height:10px;border-radius:50%;background:#22c55e;box-shadow:0 0 8px #22c55e}"
"header h1{font-size:18px;font-weight:600;color:#fff}"
"header span{color:#6b7280;font-size:13px}"
".chat{flex:1;overflow-y:auto;padding:20px;display:flex;flex-direction:column;gap:12px}"
".msg{max-width:80%;padding:12px 16px;border-radius:16px;font-size:14px;line-height:1.5;word-wrap:break-word;white-space:pre-wrap}"
".msg.user{align-self:flex-end;background:#2563eb;color:#fff;border-bottom-right-radius:4px}"
".msg.dot{align-self:flex-start;background:#1f2937;color:#e5e7eb;border-bottom-left-radius:4px}"
".msg.dot .name{font-size:11px;color:#8899aa;margin-bottom:4px;font-weight:600}"
".msg.system{align-self:center;background:transparent;color:#6b7280;font-size:12px;padding:4px}"
".typing{align-self:flex-start;color:#6b7280;font-size:13px;padding:4px 16px}"
"footer{background:#111827;padding:12px 20px;border-top:1px solid #1f2937;display:flex;gap:10px}"
"footer input{flex:1;background:#1f2937;border:1px solid #374151;border-radius:12px;padding:12px 16px;color:#fff;font-size:14px;outline:none}"
"footer input:focus{border-color:#2563eb}"
"footer button{background:#2563eb;color:#fff;border:none;border-radius:12px;padding:12px 20px;font-size:14px;cursor:pointer;font-weight:500}"
"footer button:hover{background:#1d4ed8}"
"footer button:disabled{background:#374151;cursor:default}"
".status{display:flex;gap:16px;padding:8px 20px;background:#0d1117;font-size:12px;color:#6b7280;border-top:1px solid #1f2937}"
".status span{display:flex;align-items:center;gap:4px}"
"</style></head><body>"
"<header>"
"<div class='dot' id='status-dot'></div>"
"<h1>DOT</h1>"
"<span>AiSync Services</span>"
"</header>"
"<div class='chat' id='chat'></div>"
"<div class='status'>"
"<span id='ws-status'>Connecting...</span>"
"<span id='ip-info'></span>"
"</div>"
"<footer>"
"<input id='input' placeholder='Talk to DOT...' autocomplete='off'>"
"<button id='send' onclick='sendMsg()'>Send</button>"
"</footer>"
"<script>"
"const chat=document.getElementById('chat');"
"const input=document.getElementById('input');"
"const sendBtn=document.getElementById('send');"
"const wsStatus=document.getElementById('ws-status');"
"const statusDot=document.getElementById('status-dot');"
"const ipInfo=document.getElementById('ip-info');"
"let ws,typing=null;"
""
"function connect(){"
"const host=location.host||'192.168.1.209:18789';"
"wsStatus.textContent='Connecting...';"
"statusDot.style.background='#eab308';"
"ws=new WebSocket('ws://'+host+'/ws');"
"ws.onopen=()=>{"
"wsStatus.textContent='Connected';"
"statusDot.style.background='#22c55e';"
"ipInfo.textContent=host;"
"addMsg('system','Connected to DOT');"
"sendBtn.disabled=false;"
"};"
"ws.onclose=()=>{"
"wsStatus.textContent='Disconnected';"
"statusDot.style.background='#ef4444';"
"sendBtn.disabled=true;"
"setTimeout(connect,3000);"
"};"
"ws.onerror=()=>{};"
"ws.onmessage=(e)=>{"
"clearTyping();"
"try{const d=JSON.parse(e.data);if(d.content)addMsg('dot',d.content);}catch(x){addMsg('dot',e.data);}"
"};"
"}"
""
"function addMsg(type,text){"
"const d=document.createElement('div');"
"d.className='msg '+type;"
"if(type==='dot'){d.innerHTML='<div class=\"name\">DOT</div>'+escHtml(text);}else if(type==='user'){d.textContent=text;}else{d.textContent=text;}"
"chat.appendChild(d);"
"chat.scrollTop=chat.scrollHeight;"
"}"
""
"function escHtml(t){const d=document.createElement('span');d.textContent=t;return d.innerHTML;}"
""
"function showTyping(){"
"if(typing)return;"
"typing=document.createElement('div');"
"typing.className='typing';"
"typing.textContent='DOT is thinking...';"
"chat.appendChild(typing);"
"chat.scrollTop=chat.scrollHeight;"
"}"
""
"function clearTyping(){if(typing){typing.remove();typing=null;}}"
""
"function sendMsg(){"
"const t=input.value.trim();"
"if(!t||!ws||ws.readyState!==1)return;"
"addMsg('user',t);"
"ws.send(JSON.stringify({type:'message',content:t}));"
"input.value='';"
"showTyping();"
"}"
""
"input.addEventListener('keydown',(e)=>{if(e.key==='Enter')sendMsg();});"
"connect();"
"</script></body></html>";

static esp_err_t dashboard_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, DASHBOARD_HTML, strlen(DASHBOARD_HTML));
}

/* ── Server start ───────────────────────────── */
esp_err_t ws_server_start(void)
{
    memset(s_clients, 0, sizeof(s_clients));

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = MIMI_WS_PORT;
    config.ctrl_port = MIMI_WS_PORT + 1;
    config.max_open_sockets = MIMI_WS_MAX_CLIENTS;
    config.max_uri_handlers = 4;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket server: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Dashboard page */
    httpd_uri_t dash_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = dashboard_handler,
    };
    httpd_register_uri_handler(s_server, &dash_uri);

    /* WebSocket endpoint */
    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(s_server, &ws_uri);

    ESP_LOGI(TAG, "WebSocket server started on port %d", MIMI_WS_PORT);
    return ESP_OK;
}

esp_err_t ws_server_send(const char *chat_id, const char *text)
{
    if (!s_server) return ESP_ERR_INVALID_STATE;

    ws_client_t *client = find_client_by_chat_id(chat_id);
    if (!client) {
        ESP_LOGW(TAG, "No WS client with chat_id=%s", chat_id);
        return ESP_ERR_NOT_FOUND;
    }

    /* Build response JSON */
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", "response");
    cJSON_AddStringToObject(resp, "content", text);
    cJSON_AddStringToObject(resp, "chat_id", chat_id);

    char *json_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    if (!json_str) return ESP_ERR_NO_MEM;

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)json_str,
        .len = strlen(json_str),
    };

    esp_err_t ret = httpd_ws_send_frame_async(s_server, client->fd, &ws_pkt);
    free(json_str);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send to %s: %s", chat_id, esp_err_to_name(ret));
        remove_client(client->fd);
    }

    return ret;
}

esp_err_t ws_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "WebSocket server stopped");
    }
    return ESP_OK;
}
