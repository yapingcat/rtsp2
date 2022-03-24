#ifndef RTSP2_SDP_H
#define RTSP2_SDP_H

#include <string>
#include <tuple>
#include <vector>

namespace rtsp2
{

    // Session description
    //     v=  (protocol version)
    //     o=  (originator and session identifier)
    //     s=  (session name)
    //     i=* (session information)
    //     u=* (URI of description)
    //     e=* (email address)
    //     p=* (phone number)
    //     c=* (connection information -- not required if included in
    //         all media)
    //     b=* (zero or more bandwidth information lines)
    //     One or more time descriptions ("t=" and "r=" lines; see below)
    //     z=* (time zone adjustments)
    //     k=* (encryption key)
    //     a=* (zero or more session attribute lines)
    //     Zero or more media descriptions

    // Time description
    //     t=  (time the session is active)
    //     r=* (zero or more repeat times)

    // Media description, if present
    //     m=  (media name and transport address)
    //     i=* (media title)
    //     c=* (connection information -- optional if included at
    //         session level)
    //     b=* (zero or more bandwidth information lines)
    //     k=* (encryption key)
    //     a=* (zero or more media attribute lines)

    class SdpPrinter
    {
    public:
        virtual void print() = 0;
    };

    struct Origin
    {
        std::string username = "";
        std::string sess_id;
        std::string sess_version;
        std::string nettype;
        std::string addrtype;
        std::string unicast_address;

        explicit operator bool() { return !username.empty(); }

        void parser(const std::string &orgin);
    };

    struct ConnectionData
    {
        std::string nettype = "";
        std::string addrtype;
        std::string connection_address;

        explicit operator bool() { return !nettype.empty(); }

        void parser(const std::string &connectionData);
    };

    struct BandWidth
    {
        std::string bwtype = "";
        std::string bandwidth;

        explicit operator bool() { return !bwtype.empty(); }

        void parser(const std::string &bandWidth);
    };

    struct Timing
    {
        std::string start_time = "";
        std::string stop_time;

        explicit operator bool() { return !start_time.empty(); }

        void parser(const std::string &timing);
    };

    struct RepeatTimes
    {
        std::string repeat_interval = "";
        std::string active_duration;
        std::string offsets_from_start_time;

        explicit operator bool() { return !repeat_interval.empty(); }

        void parser(const std::string &timing);
    };

    struct TimeZones
    {
        std::vector<std::tuple<std::string, std::string>> adj_Offset; // z=<adjustment time> <offset> <adjustment time> <offset> ....

        explicit operator bool() { return !adj_Offset.empty(); }

        void parser(const std::string &timeZones);
    };

    struct EncryptionKeys
    {
        std::string method = "";
        std::string encryption_key;

        explicit operator bool() { return !method.empty(); }

        void parser(const std::string &encryption);
    };

    struct Media
    {
        std::string media = "";
        std::vector<int> ports;
        std::string proto;
        std::vector<int> fmts;

        explicit operator bool() { return !media.empty(); }

        void parser(const std::string &media);
    };

    struct Attribute
    {
        std::string name = "";
        std::string value = "";

        void parser(const std::string &attr);

        explicit operator bool() { return !name.empty(); }
    };

    struct RtpMap : public Attribute
    {
        int pt;
        std::string encodeName_;
        int clockRate;
        std::string encodeParmeters;
    };

    struct Ftmp : public Attribute
    {
        int format;
        std::string formatSpecificParameters;
    };

    struct Quality : public Attribute
    {
        int quality;
    };

    struct Framerate : public Attribute
    {
        int frameRate;
    };

    struct lang : public Attribute
    {
        std::string languageTag;
    };

    struct cat : public Attribute
    {
        std::string category;
    };

    struct keywds : public Attribute
    {
        std::string keywords;
    };

    struct SessionDescribe
    {
        int version = -1;
        Origin origin;
        std::string sessionName;
        std::string sessionInfo;
        std::string Uri;
        std::string email_address;
        std::string phone_number;
        ConnectionData conn;
        TimeZones zones;
        EncryptionKeys keys;
        std::string aggregateUrl;
        std::vector<Attribute> attrs;
    };

    struct TimeDescription
    {
        Timing time;
        RepeatTimes repeat;
    };

    struct MediaDescription
    {
        Media media;
        std::string mediaTitle;
        ConnectionData conn;
        BandWidth bw;
        EncryptionKeys encryKeys;
        std::string aggregateUrl;
        std::vector<Attribute> attrs;
    };

    struct Sdp
    {
        SessionDescribe sessionDescription;
        TimeDescription timeDescription;
        std::vector<MediaDescription> mediaDescriptions;
        std::string toString();
    };

    Sdp parser(const std::string &sdp);

} // namespace rtsp2

#endif
