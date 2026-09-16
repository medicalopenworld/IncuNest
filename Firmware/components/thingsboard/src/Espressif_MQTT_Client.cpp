// Header include.
#include "Espressif_MQTT_Client.h"

#if THINGSBOARD_USE_ESP_MQTT

// PARCHE INCUNEST (7): cabeceras para la comprobacion de heap previa a crear
// el cliente. Ver el bloque largo en connect().
#include <esp_heap_caps.h>
#include <esp_log.h>

// The error integer -1 means a general failure while handling the mqtt client,
// where as -2 means that the outbox is filled and the message can therefore not be sent.
// Therefore we have to check if the value is smaller or equal to the MQTT_FAILURE_MESSAGE_ID,
// to ensure other errors are indentified as well
constexpr int MQTT_FAILURE_MESSAGE_ID = -1;

// PARCHE INCUNEST (7): etiqueta de log y holguras de la comprobacion de heap.
//
// MARGIN_BLOCK es holgura sobre el bloque contiguo mas grande; MARGIN_TOTAL
// cubre lo pequeno que esp-mqtt reserva ademas de los dos bufers (estructura
// del cliente, almacen de configuracion, cadenas de host/uri/credenciales y
// outbox). No cubre la pila de la tarea que crea esp_mqtt_client_start(), que
// se reserva despues: si esa falla, start() devuelve error y se trata solo
// como un fallo de conexion, sin panico.
constexpr char INCUNEST_MQTT_TAG[] = "tb_mqtt";
constexpr size_t INCUNEST_MQTT_MARGIN_BLOCK = 1024U;
constexpr size_t INCUNEST_MQTT_MARGIN_TOTAL = 4096U;

Espressif_MQTT_Client *Espressif_MQTT_Client::m_instance = nullptr;

Espressif_MQTT_Client::Espressif_MQTT_Client() :
    m_received_data_callback(nullptr),
    m_connected_callback(nullptr),
    m_connected(false),
    m_enqueue_messages(false),
    m_mqtt_configuration(),
    m_mqtt_client(nullptr)
{
    m_instance = this;
}

Espressif_MQTT_Client::~Espressif_MQTT_Client() {
    m_instance = nullptr;
    (void)esp_mqtt_client_destroy(m_mqtt_client);
}

bool Espressif_MQTT_Client::set_server_certificate(const char *server_certificate_pem) {
    // ESP_IDF_VERSION_MAJOR Version 5 is a major breaking changes were the complete esp_mqtt_client_config_t structure changed completely.
    // Because PEM format is expected for the server certificate we do not need to set the certificate_len,
    // because the PEM format expects a null-terminated string.
#if ESP_IDF_VERSION_MAJOR < 5
    m_mqtt_configuration.cert_pem = server_certificate_pem;
#else
    m_mqtt_configuration.broker.verification.certificate = server_certificate_pem;
#endif // ESP_IDF_VERSION_MAJOR < 5
    return update_configuration();
}

bool Espressif_MQTT_Client::set_keep_alive_timeout(const uint16_t& keep_alive_timeout_seconds) {
    // ESP_IDF_VERSION_MAJOR Version 5 is a major breaking changes were the complete esp_mqtt_client_config_t structure changed completely
#if ESP_IDF_VERSION_MAJOR < 5
    m_mqtt_configuration.keepalive = keep_alive_timeout_seconds;
#else
    m_mqtt_configuration.session.keepalive = keep_alive_timeout_seconds;
#endif // ESP_IDF_VERSION_MAJOR < 5
    return update_configuration();
}

bool Espressif_MQTT_Client::set_disable_keep_alive(const bool& disable_keep_alive) {
    // ESP_IDF_VERSION_MAJOR Version 5 is a major breaking changes were the complete esp_mqtt_client_config_t structure changed completely
#if ESP_IDF_VERSION_MAJOR < 5
    m_mqtt_configuration.disable_keepalive = disable_keep_alive;
#else
    m_mqtt_configuration.session.disable_keepalive = disable_keep_alive;
#endif // ESP_IDF_VERSION_MAJOR < 5
    return update_configuration();
}

bool Espressif_MQTT_Client::set_disable_auto_reconnect(const bool& disable_auto_reconnect) {
    // ESP_IDF_VERSION_MAJOR Version 5 is a major breaking changes were the complete esp_mqtt_client_config_t structure changed completely
#if ESP_IDF_VERSION_MAJOR < 5
    m_mqtt_configuration.disable_auto_reconnect = disable_auto_reconnect;
#else
    m_mqtt_configuration.network.disable_auto_reconnect = disable_auto_reconnect;
#endif // ESP_IDF_VERSION_MAJOR < 5
    return update_configuration();
}

bool Espressif_MQTT_Client::set_mqtt_task_configuration(const uint8_t& priority, const uint16_t& stack_size) {
    // ESP_IDF_VERSION_MAJOR Version 5 is a major breaking changes were the complete esp_mqtt_client_config_t structure changed completely
#if ESP_IDF_VERSION_MAJOR < 5
    m_mqtt_configuration.task_prio = priority;
    m_mqtt_configuration.task_stack = stack_size;
#else
    m_mqtt_configuration.task.priority = priority;
    m_mqtt_configuration.task.stack_size = stack_size;
#endif // ESP_IDF_VERSION_MAJOR < 5
    return update_configuration();
}

bool Espressif_MQTT_Client::set_reconnect_timeout(const uint16_t& reconnect_timeout_milliseconds) {
    // ESP_IDF_VERSION_MAJOR Version 5 is a major breaking changes were the complete esp_mqtt_client_config_t structure changed completely
#if ESP_IDF_VERSION_MAJOR < 5
    m_mqtt_configuration.reconnect_timeout_ms = reconnect_timeout_milliseconds;
#else
    m_mqtt_configuration.network.reconnect_timeout_ms = reconnect_timeout_milliseconds;
#endif // ESP_IDF_VERSION_MAJOR < 5
    return update_configuration();
}

bool Espressif_MQTT_Client::set_network_timeout(const uint16_t& network_timeout_milliseconds) {
    // ESP_IDF_VERSION_MAJOR Version 5 is a major breaking changes were the complete esp_mqtt_client_config_t structure changed completely
#if ESP_IDF_VERSION_MAJOR < 5
    m_mqtt_configuration.network_timeout_ms = network_timeout_milliseconds;
#else
    m_mqtt_configuration.network.timeout_ms = network_timeout_milliseconds;
#endif // ESP_IDF_VERSION_MAJOR < 5
    return update_configuration();
}

void Espressif_MQTT_Client::set_enqueue_messages(const bool& enqueue_messages) {
    m_enqueue_messages = enqueue_messages;
}

void Espressif_MQTT_Client::set_data_callback(data_function callback) {
    m_received_data_callback = callback;
}

void Espressif_MQTT_Client::set_connect_callback(connect_function callback) {
    m_connected_callback = callback;
}

bool Espressif_MQTT_Client::set_buffer_size(const uint16_t& buffer_size) {
    // ESP_IDF_VERSION_MAJOR Version 5 is a major breaking changes were the complete esp_mqtt_client_config_t structure changed completely
#if ESP_IDF_VERSION_MAJOR < 5
    m_mqtt_configuration.buffer_size = buffer_size;
#else
    m_mqtt_configuration.buffer.size = buffer_size;
#endif // ESP_IDF_VERSION_MAJOR < 5

    // Calls esp_mqtt_set_config(), which should adjust the underlying mqtt client to the changed values.
    // Which it does but not for the buffer_size, this results in the buffer size only being able to be changed when initally creating the mqtt client.
    // If the mqtt client is reinitalized this causes disconnected and reconnects tough and the connection becomes unstable.
    // Therefore this workaround can also not be used. Instead we expect the esp_mqtt_set_config(), to do what the name implies and therefore still call it
    // and created an issue revolving around the aformentioned problem so it might get fixed in future version of the esp_mqtt client.
    // See https://github.com/espressif/esp-mqtt/issues/267 for more information on the issue 
    return update_configuration();
}

uint16_t Espressif_MQTT_Client::get_buffer_size() {
    // ESP_IDF_VERSION_MAJOR Version 5 is a major breaking changes were the complete esp_mqtt_client_config_t structure changed completely
#if ESP_IDF_VERSION_MAJOR < 5
    return m_mqtt_configuration.buffer_size;
#else
    return m_mqtt_configuration.buffer.size;
#endif // ESP_IDF_VERSION_MAJOR < 5
}

void Espressif_MQTT_Client::set_server(const char *domain, const uint16_t& port) {
    // ESP_IDF_VERSION_MAJOR Version 5 is a major breaking changes were the complete esp_mqtt_client_config_t structure changed completely
#if ESP_IDF_VERSION_MAJOR < 5
    m_mqtt_configuration.host = domain;
    m_mqtt_configuration.port = port;
    // Decide transport depending on if a certificate was passed, because the set_server() method is called in the connect method meaning if the certificate has not been set yet,
    // it is to late as we attempt to establish the connection in the connect() method which is called directly after this one.
    const bool transport_over_sll = m_mqtt_configuration.cert_pem != nullptr;
#else
    m_mqtt_configuration.broker.address.hostname = domain;
    m_mqtt_configuration.broker.address.port = port;
    // Decide transport depending on if a certificate was passed, because the set_server() method is called in the connect method meaning if the certificate has not been set yet,
    // it is to late as we attempt to establish the connection in the connect() method which is called directly after this one.
    const bool transport_over_sll = m_mqtt_configuration.broker.verification.certificate != nullptr;
#endif // ESP_IDF_VERSION_MAJOR < 5

    const esp_mqtt_transport_t transport = (transport_over_sll ? esp_mqtt_transport_t::MQTT_TRANSPORT_OVER_SSL : esp_mqtt_transport_t::MQTT_TRANSPORT_OVER_TCP);

    // ESP_IDF_VERSION_MAJOR Version 5 is a major breaking changes were the complete esp_mqtt_client_config_t structure changed completely
#if ESP_IDF_VERSION_MAJOR < 5
    m_mqtt_configuration.transport = transport;
#else
    m_mqtt_configuration.broker.address.transport = transport;
#endif // ESP_IDF_VERSION_MAJOR < 5
}

bool Espressif_MQTT_Client::connect(const char *client_id, const char *user_name, const char *password) {
    // ESP_IDF_VERSION_MAJOR Version 5 is a major breaking changes were the complete esp_mqtt_client_config_t structure changed completely
#if ESP_IDF_VERSION_MAJOR < 5
    m_mqtt_configuration.client_id = client_id;
    m_mqtt_configuration.username = user_name;
    m_mqtt_configuration.password = password;
#else
    m_mqtt_configuration.credentials.client_id = client_id;
    m_mqtt_configuration.credentials.username = user_name;
    m_mqtt_configuration.credentials.authentication.password = password;
#endif // ESP_IDF_VERSION_MAJOR < 5
    // Update configuration is called to ensure that if we connected previously and call connect again with other credentials,
    // then we also update the client_id, username and password we connect with. Especially important for the provisioning workflow to work correctly
    update_configuration();

    // Check wheter the client has been initalzed before already, it it has we do not want to reinitalize,
    // but simply force reconnection with the client because it has lost that connection
    if (m_mqtt_client != nullptr) {
        const esp_err_t error = esp_mqtt_client_reconnect(m_mqtt_client);
        return error == ESP_OK;
    }

    // PARCHE INCUNEST (7): no crear el cliente si la RAM interna no da para sus
    // bufers.
    //
    // esp-mqtt reserva DOS bloques de `buffer.size` al configurar el cliente, el
    // de entrada y el de salida. Cuando el de entrada no cabe,
    // `esp_mqtt_set_config()` salta a su etiqueta de error a traves del macro
    // ESP_MEM_CHECK, que solo imprime y hace `goto`: NO toca la variable `err`,
    // inicializada a ESP_OK. O sea que la funcion DEVUELVE ESP_OK despues de
    // haber llamado a `esp_mqtt_destroy_config()`, que deja `client->config` a
    // nulo. `esp_mqtt_client_init()` se lo cree, crea el bucle de eventos en
    // `&client->config->event_loop_handle` -- direccion 0, de ahi el
    // "event_loop was NULL" del log -- y devuelve un handle NO nulo a medio
    // construir. El primer uso lo desreferencia y la placa entra en panico.
    //
    // Medido en banco el 2026-09-16 (SN 353), a los 30 s de arranque:
    //
    //   E mqtt_client: esp_mqtt_set_config(492): Memory exhausted
    //   E event: event_loop was NULL
    //   Guru Meditation Error: Core 1 panic'ed (LoadProhibited) EXCVADDR 0
    //     esp_mqtt_client_register_event -> Espressif_MQTT_Client::connect
    //
    // El desensamblado lo confirma: la instruccion que falla es la carga de
    // `client->config->event_loop_handle` con `client->config` a cero, ya
    // pasada la comprobacion de handle nulo que si tiene esa funcion. Por eso
    // mirar solo el valor de retorno de init() no basta.
    //
    // El reinicio no se queda en un reloj perdido: `initGPRS()` ve un reset
    // anormal y borra la tarea GPRS de la sesion, asi que un fallo de heap
    // llegando por WiFi deja la unidad tambien sin celular hasta que alguien le
    // quite la corriente.
    //
    // Por que pasa aqui: la motherBoard no tiene PSRAM y el bufer se
    // dimensiona para que quepa un trozo de OTA entero (TB_MQTT_BUFFER_WIFI,
    // 4352 B), pedidos contiguos en el peor momento, con WiFi y TLS ya en pie.
    // Comprobarlo ANTES es lo unico que evita el panico sin depender de las
    // interioridades de esp-mqtt. Si no hay sitio se devuelve false y el
    // reintento normal de ThingsBoard lo vuelve a probar mas tarde, que es
    // justo lo que este metodo promete a quien lo llama.
#if ESP_IDF_VERSION_MAJOR < 5
    size_t const buffer_size = m_mqtt_configuration.buffer_size;
#else
    size_t const buffer_size = m_mqtt_configuration.buffer.size;
#endif // ESP_IDF_VERSION_MAJOR < 5
    // Se piden dos bufers del mismo tamano: `out_size` a cero significa "como
    // el de entrada" y este SDK nunca configura otra cosa (set_buffer_size()
    // solo escribe el de entrada).
    size_t const largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    size_t const total_free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    if (largest_block < buffer_size + INCUNEST_MQTT_MARGIN_BLOCK ||
        total_free < (2U * buffer_size) + INCUNEST_MQTT_MARGIN_TOTAL) {
        ESP_LOGE(INCUNEST_MQTT_TAG,
                 "sin RAM para el cliente MQTT: bufer %u B x2, bloque mayor %u B, "
                 "libre %u B; no se crea y se reintenta luego",
                 static_cast<unsigned>(buffer_size),
                 static_cast<unsigned>(largest_block),
                 static_cast<unsigned>(total_free));
        return false;
    }

    // PARCHE INCUNEST (7): deja constancia del margen real con el que se crea
    // el cliente. Solo se llega aqui cuando todavia no existe, o sea una vez
    // por cliente y transporte, salvo que la creacion este fallando -- que es
    // justo cuando interesa ver las cifras.
    ESP_LOGI(INCUNEST_MQTT_TAG,
             "creando cliente MQTT: bufer %u B x2, bloque mayor %u B, libre %u B",
             static_cast<unsigned>(buffer_size),
             static_cast<unsigned>(largest_block),
             static_cast<unsigned>(total_free));

    // The client is first initalized once the connect has actually been called, this is done because the passed setting are required for the client inizialitation structure,
    // additionally before we attempt to connect with the client we have to ensure it is configued by then.
    m_mqtt_client = esp_mqtt_client_init(&m_mqtt_configuration);

    // PARCHE INCUNEST (7): init() devuelve nulo en varios caminos de fallo y
    // upstream no lo miraba. No cubre el handle a medio construir de arriba,
    // pero un nulo aqui llegaria igualmente a esp_mqtt_client_start().
    if (m_mqtt_client == nullptr) {
        ESP_LOGE(INCUNEST_MQTT_TAG, "esp_mqtt_client_init() devolvio nulo");
        return false;
    }

    // PARCHE INCUNEST (4): se pasa `this` como handler_args en vez de nullptr.
    // Upstream lo dejaba a nullptr y el manejador estatico despachaba siempre
    // contra m_instance, un unico puntero estatico que el constructor pisa.
    // Con UNA instancia da igual; este firmware crea DOS (mqttClientGPRS en
    // GPRS.cpp y mqttClientWIFI en Wifi_OTA.cpp, uno por transporte), asi que
    // el ultimo construido se quedaba TODOS los eventos de los dos clientes.
    //
    // Consecuencia medida en banco el 2026-09-15: por GPRS, el
    // MQTT_EVENT_CONNECTED del cliente celular se entregaba al objeto de WiFi,
    // el m_connected del de GPRS no se ponia nunca a true y connected() mentia
    // para siempre. El broker veia la sesion viva (ThingsBoard marcaba el
    // equipo activo) mientras el firmware reintentaba cada 30 s, no suscribia
    // ni un RPC (el servidor devolvia 409) y no pedia la OTA jamas. Los datos
    // entrantes -- RPC y trozos de OTA -- tambien iban al cliente equivocado.
    //
    // esp_mqtt_client_register_event ya reenvia handler_args al manejador, asi
    // que basta con usarlo; m_instance se conserva como respaldo para no
    // romper a quien registre con nullptr.
    esp_err_t error = esp_mqtt_client_register_event(m_mqtt_client, esp_mqtt_event_id_t::MQTT_EVENT_ANY, Espressif_MQTT_Client::static_mqtt_event_handler, this);

    if (error != ESP_OK) {
        return false;
    }

    error = esp_mqtt_client_start(m_mqtt_client);
    return error == ESP_OK;
}

void Espressif_MQTT_Client::disconnect() {
    (void)esp_mqtt_client_disconnect(m_mqtt_client);
}

bool Espressif_MQTT_Client::loop() {
    // Unused because the esp mqtt client uses its own task to handle receiving and sending of data, therefore we do not need to do anything in the loop method.
    // Because the loop method is meant for clients that do not have their own process method but instead rely on the upper level code calling a loop method to provide processsing time.
    return m_connected;
}

bool Espressif_MQTT_Client::publish(const char *topic, const uint8_t *payload, const size_t& length) {
    int message_id = MQTT_FAILURE_MESSAGE_ID;

    if (m_enqueue_messages) {
        message_id = esp_mqtt_client_enqueue(m_mqtt_client, topic, reinterpret_cast<const char*>(payload), length, 0U, 0U, true);
        return message_id > MQTT_FAILURE_MESSAGE_ID;
    }

    // The blocking version esp_mqtt_client_publish() it is sent directly from the users task context.
    // This way is used to send messages to the cloud, because like that no internal buffer has to be used to store the message until it should be sent,
    // because all messages are sent with QoS level 0. If this is not wanted esp_mqtt_client_enqueue() could be used with store = true,
    // to ensure the sending is done in the mqtt event context instead of the users task context.
    // Allows to use the publish method without having to worry about any CPU overhead, so it can even be used in callbacks or high priority tasks, without starving other tasks,
    // but compared to the other method esp_mqtt_client_enqueue() requires to save the message in the outbox, which increases the memory requirements for the internal buffer size
    message_id = esp_mqtt_client_publish(m_mqtt_client, topic, reinterpret_cast<const char*>(payload), length, 0U, 0U);
    return message_id > MQTT_FAILURE_MESSAGE_ID;
}

bool Espressif_MQTT_Client::subscribe(const char *topic) {
    // The esp_mqtt_client_subscribe method does not return false, if we send a subscribe request while not being connected to a broker,
    // so we have to check for that case to ensure the end user is informed that their subscribe request could not be sent and has been ignored.
    if (!connected()) {
        return false;
    }
    const int message_id = esp_mqtt_client_subscribe(m_mqtt_client, topic, 0U);
    return message_id > MQTT_FAILURE_MESSAGE_ID;
}

bool Espressif_MQTT_Client::unsubscribe(const char *topic) {
    // The esp_mqtt_client_unsubscribe method does not return false, if we send a unsubscribe request while not being connected to a broker,
    // so we have to check for that case to ensure the end user is informed that their unsubscribe request could not be sent and has been ignored.
    if (!connected()) {
        return false;
    }
    const int message_id = esp_mqtt_client_unsubscribe(m_mqtt_client, topic);
    return message_id > MQTT_FAILURE_MESSAGE_ID;
}

bool Espressif_MQTT_Client::connected() {
    return m_connected;
}

bool Espressif_MQTT_Client::update_configuration() {
    // Check if the client has been initalized, because if it did not the value should still be nullptr
    // and updating the config makes no sense because the changed settings will be applied anyway when the client is first intialized
    if (m_mqtt_client == nullptr) {
        return true;
    }

    const esp_err_t error = esp_mqtt_set_config(m_mqtt_client, &m_mqtt_configuration);
    return error == ESP_OK;
}

void Espressif_MQTT_Client::mqtt_event_handler(void *handler_args, esp_event_base_t base, const esp_mqtt_event_id_t& event_id, void *event_data) {
    const esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (event_id) {
        case esp_mqtt_event_id_t::MQTT_EVENT_CONNECTED:
            m_connected = true;
            m_connected_callback();
            break;
        case esp_mqtt_event_id_t::MQTT_EVENT_DISCONNECTED:
            m_connected = false;
            break;
        case esp_mqtt_event_id_t::MQTT_EVENT_SUBSCRIBED:
            // Nothing to do
            break;
        case esp_mqtt_event_id_t::MQTT_EVENT_UNSUBSCRIBED:
            // Nothing to do
            break;
        case esp_mqtt_event_id_t::MQTT_EVENT_PUBLISHED:
            // Nothing to do
            break;
        case esp_mqtt_event_id_t::MQTT_EVENT_DATA:
            // Check wheter the given message has not bee received completly, but instead would be received in multiple chunks,
            // if it were we discard the message because receiving a message over multiple chunks is currently not supported
            if (event->data_len != event->total_data_len) {
                break;
            }

            if (m_received_data_callback != nullptr) {
                m_received_data_callback(event->topic, reinterpret_cast<uint8_t*>(event->data), event->data_len);
            }
            break;
        case esp_mqtt_event_id_t::MQTT_EVENT_ERROR:
            // Nothing to do
            break;
        case esp_mqtt_event_id_t::MQTT_EVENT_BEFORE_CONNECT:
            // Nothing to do
            break;
        default:
            // Nothing to do
            break;
    }
}

void Espressif_MQTT_Client::static_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    // PARCHE INCUNEST (4): el evento se despacha al cliente que lo registro,
    // no al ultimo construido. Ver el comentario largo en connect().
    Espressif_MQTT_Client *instance = static_cast<Espressif_MQTT_Client *>(handler_args);
    if (instance == nullptr) {
        // Respaldo para registros hechos con nullptr (comportamiento upstream).
        instance = m_instance;
    }
    if (instance == nullptr) {
        return;
    }

    instance->mqtt_event_handler(handler_args, base, static_cast<esp_mqtt_event_id_t>(event_id), event_data);
}

#endif // THINGSBOARD_USE_ESP_MQTT
