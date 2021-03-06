#include "connection_visitor.h"
#include "clock.h"

namespace posix_quic {

class NoAckAlarmDelegate : public QuicAlarm::Delegate {
public:
    explicit NoAckAlarmDelegate(QuicConnectionVisitor *visitor)
        : visitor_(visitor) {}

    void OnAlarm() override {
        visitor_->CheckForNoAckTimeout();
    }

private:
    QuicConnectionVisitor* visitor_;

    DISALLOW_COPY_AND_ASSIGN(NoAckAlarmDelegate);
};

QuicConnectionVisitor::QuicConnectionVisitor()
    : lastAckTime_(QuicTime::Zero()), lastSendTime_(QuicTime::Zero())
{
}
QuicConnectionVisitor::~QuicConnectionVisitor()
{
}

void QuicConnectionVisitor::CheckForNoAckTimeout()
{
    QuicTime::Delta allow_duration = QuicTime::Delta::FromSeconds(opts_->GetOption(sockopt_ack_timeout_secs));
    if (allow_duration.IsZero())
        return ;

    if (lastSendTime_ > lastAckTime_) {
        QuicTime now = QuicClockImpl::getInstance().Now();
        QuicTime::Delta ack_duration = now - lastSendTime_;
        if (ack_duration > allow_duration) {
            DebugPrint(dbg_close | dbg_ack_timeout, 
                    "CloseConnection by ack timeout. fd = %d. now = %ld, lastAck = %ld, lastSend = %ld",
                    parent_->Fd(),
                    (long)(now - QuicTime::Zero()).ToMilliseconds(),
                    (long)(lastAckTime_ - QuicTime::Zero()).ToMilliseconds(),
                    (long)(lastSendTime_ - QuicTime::Zero()).ToMilliseconds());
            connection_->CloseConnection(net::QUIC_NETWORK_ACK_TIMEOUT, "ack timeout",
                      net::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET_WITH_NO_ACK);
            return ;
        }
    }

    SetNoAckAlarm();
}

void QuicConnectionVisitor::SetNoAckAlarm()
{
    int secs = opts_->GetOption(sockopt_ack_timeout_secs);
    QuicTime::Delta allow_duration = QuicTime::Delta::FromSeconds(secs);
    if (allow_duration.IsZero())
        return ;

    QuicTime timeOfLast = lastSendTime_ > lastAckTime_ ? lastSendTime_ : QuicClockImpl::getInstance().Now();
    QuicTime deadline = timeOfLast + allow_duration;

    DebugPrint(dbg_ack_timeout, "fd = %d, secs = %d", parent_->Fd(), secs);
    noAckAlarm_->Update(deadline, QuicTime::Delta::Zero());
}

void QuicConnectionVisitor::CancelNoAckAlarm()
{
    noAckAlarm_->Cancel();
}

void QuicConnectionVisitor::Bind(QuicConnection * connection,
        QuicSocketOptions * opts, FdBase * parent,
        std::shared_ptr<PosixQuicPacketTransport> packetTransport)
{
    connection_ = connection;
    opts_ = opts;
    parent_ = parent;
    packetTransport_ = packetTransport;
    noAckAlarm_.reset(connection->alarm_factory()->CreateAlarm(new NoAckAlarmDelegate(this)));
}

// Called when a packet has been sent.
void QuicConnectionVisitor::OnPacketSent(const SerializedPacket& serialized_packet,
        QuicPacketNumber original_packet_number,
        TransmissionType transmission_type,
        QuicTime sent_time)
{
    char frameTypes[(int)net::QuicFrameType::NUM_FRAME_TYPES + 1] = {};
    memset(frameTypes, '-', sizeof(frameTypes) - 1);
    for (auto const& frame : serialized_packet.retransmittable_frames) {
        int type = (int)frame.type;
        frameTypes[type] = '0' + type;
    }

    DebugPrint(dbg_conn_visitor | dbg_ack_timeout, "Visitor sent fd = %d, len=%d, has_ack=%d, transmission_type=%d, frames=%s",
            parent_->Fd(), (int)serialized_packet.encrypted_length,
            serialized_packet.has_ack, (int)transmission_type, frameTypes);

    if (TransmissionType::NOT_RETRANSMISSION == transmission_type) {
        if (lastSendTime_ <= lastAckTime_)
            lastSendTime_ = QuicClockImpl::getInstance().Now();
    }
}

// Called when a AckFrame has been parsed.
void QuicConnectionVisitor::OnAckFrame(const QuicAckFrame& frame)
{
    DebugPrint(dbg_conn_visitor | dbg_ack_timeout, "Visitor ack fd = %d", parent_->Fd());

    lastAckTime_ = QuicClockImpl::getInstance().Now();
}

// Called when a PING frame has been sent.
void QuicConnectionVisitor::OnPingSent()
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a packet has been received, but before it is
// validated or parsed.
void QuicConnectionVisitor::OnPacketReceived(const QuicSocketAddress& self_address,
        const QuicSocketAddress& peer_address,
        const QuicEncryptedPacket& packet)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when the unauthenticated portion of the header has been parsed.
void QuicConnectionVisitor::OnUnauthenticatedHeader(const QuicPacketHeader& header)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a packet is received with a connection id that does not
// match the ID of this connection.
void QuicConnectionVisitor::OnIncorrectConnectionId(QuicConnectionId connection_id)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when an undecryptable packet has been received.
void QuicConnectionVisitor::OnUndecryptablePacket()
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a duplicate packet has been received.
void QuicConnectionVisitor::OnDuplicatePacket(QuicPacketNumber packet_number)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when the protocol version on the received packet doensn't match
// current protocol version of the connection.
void QuicConnectionVisitor::OnProtocolVersionMismatch(ParsedQuicVersion version)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when the complete header of a packet has been parsed.
void QuicConnectionVisitor::OnPacketHeader(const QuicPacketHeader& header)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a StreamFrame has been parsed.
void QuicConnectionVisitor::OnStreamFrame(const QuicStreamFrame& frame)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());

    packetTransport_->UpdatePeerAddress(connection_->peer_address());
}

// Called when a StopWaitingFrame has been parsed.
void QuicConnectionVisitor::OnStopWaitingFrame(const QuicStopWaitingFrame& frame)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a QuicPaddingFrame has been parsed.
void QuicConnectionVisitor::OnPaddingFrame(const QuicPaddingFrame& frame)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a Ping has been parsed.
void QuicConnectionVisitor::OnPingFrame(const QuicPingFrame& frame)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a GoAway has been parsed.
void QuicConnectionVisitor::OnGoAwayFrame(const QuicGoAwayFrame& frame)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a RstStreamFrame has been parsed.
void QuicConnectionVisitor::OnRstStreamFrame(const QuicRstStreamFrame& frame)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a ConnectionCloseFrame has been parsed.
void QuicConnectionVisitor::OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a WindowUpdate has been parsed.
void QuicConnectionVisitor::OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame,
        const QuicTime& receive_time)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a BlockedFrame has been parsed.
void QuicConnectionVisitor::OnBlockedFrame(const QuicBlockedFrame& frame)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a public reset packet has been received.
void QuicConnectionVisitor::OnPublicResetPacket(const QuicPublicResetPacket& packet)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a version negotiation packet has been received.
void QuicConnectionVisitor::OnVersionNegotiationPacket(
        const QuicVersionNegotiationPacket& packet)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when the connection is closed.
void QuicConnectionVisitor::OnConnectionClosed(QuicErrorCode error,
        const QuicString& error_details,
        ConnectionCloseSource source)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when the version negotiation is successful.
void QuicConnectionVisitor::OnSuccessfulVersionNegotiation(
        const ParsedQuicVersion& version)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a CachedNetworkParameters is sent to the client.
void QuicConnectionVisitor::OnSendConnectionState(
        const CachedNetworkParameters& cached_network_params)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a CachedNetworkParameters are received from the client.
void QuicConnectionVisitor::OnReceiveConnectionState(
        const CachedNetworkParameters& cached_network_params)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when the connection parameters are set from the supplied
// |config|.
void QuicConnectionVisitor::OnSetFromConfig(const QuicConfig& config)
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when RTT may have changed, including when an RTT is read from
// the config.
void QuicConnectionVisitor::OnRttChanged(QuicTime::Delta rtt) const
{
    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

} // namespace posix_quic
