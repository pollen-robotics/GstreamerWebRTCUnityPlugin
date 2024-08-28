using System;
using UnityEngine;
using System.Threading.Tasks;
using System.Threading;
using System.Linq;
using UnityEngine.Events;
using System.Net.WebSockets;
using System.Text;

namespace GstreamerWebRTC
{
    public class Signalling
    {
        private ClientWebSocket webSocket;
        private string _remote_producer_name;
        private string _peer_id;
        private string _session_id;
        public UnityEvent<string> event_OnRemotePeerId;
        public UnityEvent<string> event_OnSDPOffer;
        public UnityEvent<string, int> event_OnICECandidate;
        public UnityEvent<string> event_OnSessionID;

        private SessionStatus sessionStatus;
        private Task task_askForList;
        private Task task_updateMessages;
        private bool tasks_running = false;
        private Uri _uri;
        private CancellationTokenSource _cts;


        public Signalling(string url, string remote_producer_name = "")
        {
            if (remote_producer_name == "")
                Debug.LogError("Remote producer name should be set for a consumer role");

            _uri = new Uri(url);

            _remote_producer_name = remote_producer_name;
            sessionStatus = SessionStatus.Ended;

            event_OnRemotePeerId = new UnityEvent<string>();
            event_OnSDPOffer = new UnityEvent<string>();
            event_OnICECandidate = new UnityEvent<string, int>();

            webSocket = new ClientWebSocket();
            _cts = new CancellationTokenSource();

        }
        public async void Connect()
        {
            // webSocket.Connect();
            await webSocket.ConnectAsync(_uri, _cts.Token);
            if (webSocket.State == WebSocketState.Open)
            {
                Debug.Log("Connected to WebSocket server.");
                tasks_running = true;
                task_updateMessages = new Task(() => UpdateMessages());
                task_updateMessages.Start();

                //if (_isProducer)
                SendMessage(MessageType.SetPeerStatus, MessageRole.Producer);
                /*else
                {
                    SendMessage(MessageType.SetPeerStatus, MessageRole.Listener);
                }*/

            }
            else
            {
                Debug.LogError("Failed to connect to WebSocket server.");
            }
        }


        public async void UpdateMessages()
        {
            //tasks_running = true;
            while (tasks_running)
            {
                // webSocket.DispatchMessageQueue();
                var responseBuffer = new byte[1024];
                Debug.Log("wait receiv");
                var result = await webSocket.ReceiveAsync(new ArraySegment<byte>(responseBuffer), _cts.Token);
                var responseMessage = Encoding.UTF8.GetString(responseBuffer, 0, result.Count);
                Debug.Log("message " + responseMessage);
                ProcessMessage(responseMessage);
                Debug.Log("here");
                //return responseMessage;
                //await Task.Delay(200);
            }
            Debug.Log("Quit update message");
        }

        private void ProcessMessage(string message)
        {
            if (message != null)
            {
                var msg = JsonUtility.FromJson<SignalingMessage>(message);

                if (msg.type == MessageType.Welcome.ToString())
                {
                    _peer_id = msg.peerId;
                    Debug.Log("peer id : " + _peer_id);
                    //if (!_isProducer)
                    //{
                    task_askForList = new Task(() => AskList());
                    task_askForList.Start();
                    //}
                }
                else if (msg.type == MessageType.PeerStatusChanged.ToString())
                {
                    Debug.Log(msg.ToString());
                    if (msg.meta?.name == _remote_producer_name && msg.roles.Contains(MessageRole.Producer.ToString()))
                    {
                        Debug.Log("Start Session");
                        SendStartSession(msg.peerId);
                    }
                }
                else if (sessionStatus == SessionStatus.Ended && msg.type == MessageType.List.ToString())
                {
                    Debug.Log("processing list..");
                    foreach (var p in msg.producers)
                    {
                        if (p.meta.name == _remote_producer_name)
                        {
                            //tasks_running = false;
                            SendStartSession(p.id);
                            event_OnRemotePeerId.Invoke(p.id);
                            sessionStatus = SessionStatus.Started;
                            break;
                        }
                    }
                }
                else if (sessionStatus == SessionStatus.Started && msg.type == MessageType.List.ToString())
                {
                    Debug.Log("Checking presence of producer " + _remote_producer_name);
                    foreach (var p in msg.producers)
                    {
                        if (p.meta.name == _remote_producer_name)
                        {
                            break;
                        }
                    }
                    Debug.LogWarning("Producer has " + _remote_producer_name + " left");
                    sessionStatus = SessionStatus.Ended;
                    _session_id = null;
                }
                else if (msg.type == MessageType.StartSession.ToString())
                {
                    _session_id = msg.sessionId;
                    //event_OnConnectionStatus.Invoke(ConnectionStatus.Ready);
                }
                else if (msg.type == MessageType.SessionStarted.ToString())
                {
                    _session_id = msg.sessionId;
                    //Debug.Log("session id: " + _session_id);
                    Debug.Log("Session started. peer id:" + msg.peerId + " session id:" + msg.sessionId);
                    //event_OnConnectionStatus.Invoke(ConnectionStatus.Ready);
                    sessionStatus = SessionStatus.Started;
                }
                else if (msg.type == MessageType.SessionEnded.ToString())
                {
                    _session_id = null;
                    Debug.Log("session ended: " + msg.sessionId);

                    //event_OnConnectionStatus.Invoke(ConnectionStatus.Waiting);
                    sessionStatus = SessionStatus.Ended;
                }
                else if (msg.type == MessageType.Peer.ToString())
                {
                    if (msg.ice.IsValid())
                    {
                        Debug.Log("received ice candidate " + msg.ice.candidate + " " + msg.ice.sdpMLineIndex);
                        event_OnICECandidate.Invoke(msg.ice.candidate, msg.ice.sdpMLineIndex);
                    }
                    else if (msg.sdp.IsValid())
                    {
                        if (msg.sdp.type == "offer")
                        {
                            Debug.Log("received offer " + msg.sdp.sdp);
                            event_OnSDPOffer.Invoke(msg.sdp.sdp);
                        }
                        else if (msg.sdp.type == "answer")
                        {
                            Debug.LogWarning("received answer");
                        }
                    }
                }
                else
                {
                    Debug.Log("Message not processed " + msg);
                }
            }
        }

        public void Close()
        {
            tasks_running = false;
            //webSocket.Close();
            //await webSocket.CloseAsync(WebSocketCloseStatus.NormalClosure, "Closing", _cts.Token);
            _cts.Cancel();
            task_askForList.Wait();
            task_updateMessages.Wait();
        }

        public async void SendSDP(string sdp_msg, string type = "answer")
        {
            string msg = JsonUtility.ToJson(new SDPMessage
            {
                type = MessageType.Peer.ToString(),
                sessionId = _session_id,
                sdp = new SdpMessage
                {
                    type = type,
                    sdp = sdp_msg,
                },
            });
            //await webSocket.SendText(msg);
            Debug.Log("Send SDP answer " + msg);
            var buffer = new ArraySegment<byte>(Encoding.UTF8.GetBytes(msg));
            await webSocket.SendAsync(buffer, WebSocketMessageType.Text, true, _cts.Token);
        }

        public async void SendICECandidate(string candidate, int mline_index)
        {
            string msg = JsonUtility.ToJson(new ICEMessage
            {
                type = MessageType.Peer.ToString(),
                sessionId = _session_id,
                ice = new ICECandidateMessage(candidate, mline_index),
            });
            // await webSocket.SendText(msg);
            var buffer = new ArraySegment<byte>(Encoding.UTF8.GetBytes(msg));
            await webSocket.SendAsync(buffer, WebSocketMessageType.Text, true, _cts.Token);
        }

        private async void AskList()
        {
            //tasks_running = true;
            while (tasks_running)
            {
                //if (sessionStatus == SessionStatus.Ended)
                //{
                Debug.Log("Ask for list");
                string msg = JsonUtility.ToJson(new SignalingMessage
                {
                    type = MessageType.List.ToString(),
                });
                // await webSocket.SendText(msg);
                var buffer = new ArraySegment<byte>(Encoding.UTF8.GetBytes(msg));
                await webSocket.SendAsync(buffer, WebSocketMessageType.Text, true, _cts.Token);
                //}
                await Task.Delay(1000);
            }
        }

        private async void SendMessage(MessageType type, MessageRole role)
        {
            Debug.Log("SetPeerStatus");
            string msg = JsonUtility.ToJson(new SignalingMessage
            {
                type = type.ToString(),
                roles = new string[] { role.ToString() },
            });
            //await webSocket.SendText(msg);
            var buffer = new ArraySegment<byte>(Encoding.UTF8.GetBytes(msg));
            await webSocket.SendAsync(buffer, WebSocketMessageType.Text, true, _cts.Token);
        }

        private async void SendStartSession(string peer_id)
        {
            Debug.Log("StartSessionMessage");
            string msg = JsonUtility.ToJson(new StartSessionMessage
            {
                type = MessageType.StartSession.ToString(),
                roles = new string[] { MessageRole.Consumer.ToString() },
                peerId = peer_id,
            });
            //await webSocket.SendText(msg);
            Debug.Log("send start session " + msg);
            var buffer = new ArraySegment<byte>(Encoding.UTF8.GetBytes(msg));
            await webSocket.SendAsync(buffer, WebSocketMessageType.Text, true, _cts.Token);
            sessionStatus = SessionStatus.Asked;
        }
    }


}