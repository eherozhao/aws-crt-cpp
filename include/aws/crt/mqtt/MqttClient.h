#pragma once
/*
 * Copyright 2010-2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#include <aws/crt/Exports.h>
#include <aws/crt/Types.h>

#include <aws/crt/io/TLSOptions.h>
#include <aws/mqtt/client.h>

#include <functional>
#include <memory>
#include <string>

namespace Aws
{
    namespace Crt
    {
        namespace Io
        {
            class ClientBootstrap;
        }

        namespace Mqtt
        {
            class MqttClient;
            class MqttConnection;

            /**
             * Invoked Upon Connection failure.
             */
            using OnConnectionFailedHandler = std::function<void(MqttConnection& connection)>;

            /**
             * Invoked when a connack message is received.
             */
            using OnConnAckHandler = std::function<void(MqttConnection& connection,
                    ReturnCode returnCode, bool sessionPresent)>;

            /**
             * Invoked when a disconnect message has been sent.
             */
            using OnDisconnectHandler = std::function<void(MqttConnection& connection)>;

            /**
             * Invoked upon receipt of a Publish message on a subscribed topic.
             */
            using OnPublishReceivedHandler = std::function<void(MqttConnection& connection, 
                const std::string& topic, const ByteBuf& payload)>;
            using OnOperationCompleteHandler = std::function<void(MqttConnection& connection, uint16_t packetId)>;

            /**
             * Represents a persistent Mqtt Connection. The memory is owned by MqttClient. This is a move only type.
             * To get a new instance of this class, see MqttClient::NewConnection.
             */
            class AWS_CRT_CPP_API MqttConnection final
            {
                friend class MqttClient;
            public:
                ~MqttConnection() = default;
                MqttConnection(const MqttConnection&) = delete;
                MqttConnection(MqttConnection&&) = default;
                MqttConnection& operator =(const MqttConnection&) = delete;
                MqttConnection& operator =(MqttConnection&&) = default;

                operator bool() const noexcept;
                int LastError() const noexcept;

                inline void SetOnConnectionFailedHandler(OnConnectionFailedHandler&& onConnectionFailed) noexcept
                {
                    m_onConnectionFailed = std::move(onConnectionFailed);
                }

                inline void SetOnConnAckHandler(OnConnAckHandler&& onConnAck) noexcept
                {
                    m_onConnAck = std::move(onConnAck);
                }

                inline void SetOnDisconnectHandler(OnDisconnectHandler&& onDisconnect) noexcept
                {
                    m_onDisconnect = std::move(onDisconnect);
                }

                /**
                 * Sets LastWill for the connection. The memory backing payload must outlive the connection.
                 */
                void SetWill(const std::string& topic, QOS qos, bool retain,
                        const ByteBuf& payload) noexcept;

                /**
                 * Sets login credentials for the connection. The must get set before the Connect call
                 * if it is to be used. 
                 */
                void SetLogin(const std::string& userName, const std::string& password) noexcept;

                /**
                 * Initiates the connection, OnConnectionFailedHandler and/or OnConnAckHandler will
                 * be invoked in an event-loop thread.
                 */
                void Connect(const std::string& clientId, bool cleanSession, uint16_t keepAliveTime) noexcept;

                /**
                 * Initiates disconnect, OnDisconnectHandler will be invoked in an event-loop thread.
                 */
                void Disconnect() noexcept;

                /**
                 * Subcribes to topicFilter. OnPublishRecievedHandler will be invoked from an event-loop
                 * thread upon an incoming Publish message. OnOperationCompleteHandler will be invoked
                 * upon receipt of a suback message.
                 */
                uint16_t Subscribe(const std::string& topicFilter, QOS qos,
                        OnPublishReceivedHandler&& onPublish,
                        OnOperationCompleteHandler&& onOpComplete) noexcept;

                /**
                 * Unsubscribes from topicFilter. OnOperationCompleteHandler will be invoked upon receipt of
                 * an unsuback message.
                 */
                uint16_t Unsubscribe(const std::string& topicFilter,
                        OnOperationCompleteHandler&& onOpComplete) noexcept;

                /**
                 * Publishes to topic. The backing memory for payload must stay available until the 
                 * OnOperationCompleteHandler has been invoked.
                 */
                uint16_t Publish(const std::string& topic, QOS qos, bool retain, const ByteBuf& payload,
                                 OnOperationCompleteHandler&& onOpComplete) noexcept;

                /**
                 * Sends a ping message.
                 */
                void Ping();

            private:
                MqttConnection(MqttClient* client, const std::string& hostName, uint16_t port,
                               const Io::SocketOptions& socketOptions,
                               Io::TlSConnectionOptions&& tlsConnOptions) noexcept;

                MqttClient *m_owningClient;
                aws_mqtt_client_connection *m_underlyingConnection;

                OnConnectionFailedHandler m_onConnectionFailed;
                OnConnAckHandler m_onConnAck;
                OnDisconnectHandler m_onDisconnect;
                int m_lastError;
                bool m_isInit;

                static void s_onConnectionFailed(aws_mqtt_client_connection* connection, int errorCode, void* userData);
                static void s_onConnAck(aws_mqtt_client_connection* connection,
                    enum aws_mqtt_connect_return_code return_code,
                    bool session_present,
                    void* user_data);
                static void s_onDisconnect(aws_mqtt_client_connection* connection, int errorCode, void* userData);
                static void s_onPublish(aws_mqtt_client_connection*connection,
                                        const aws_byte_cursor* topic,
                                        const aws_byte_cursor* payload,
                                        void* user_data);
                static void s_onOpComplete(aws_mqtt_client_connection* connection, uint16_t packetId, void* userdata);
            };

            /**
             * An MQTT client. This is a move-only type.
             */
            class AWS_CRT_CPP_API MqttClient final
            {
                friend class MqttConnection;

            public:
                /**
                 * Initialize an MqttClient using bootstrap and allocator
                 */
                MqttClient(const Io::ClientBootstrap& bootstrap, Allocator* allocator = DefaultAllocator()) noexcept;

                ~MqttClient();
                MqttClient(const MqttClient&) = delete;
                MqttClient(MqttClient&&) noexcept;
                MqttClient& operator =(const MqttClient&) = delete;
                MqttClient& operator =(MqttClient&&) noexcept;

                operator bool() const noexcept;
                int LastError() const noexcept;

                /**
                 * Create a new connection object from the client. The client must outlive
                 * all of its connection instances.
                 */
                MqttConnection NewConnection(const std::string& hostName, uint16_t port,
                        const Io::SocketOptions& socketOptions, Io::TlSConnectionOptions&& tlsConnOptions) noexcept;

            private:
                aws_mqtt_client m_client;
                int m_lastError;
                bool m_isInit;
            };
        }
    }
}
