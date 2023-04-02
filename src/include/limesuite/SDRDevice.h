#ifndef LIME_SDRDevice_H
#define LIME_SDRDevice_H

#include <vector>
#include <unordered_map>
#include <functional>
#include <string.h>
#include <string>

#include "limesuite/config.h"
#include "limesuite/IComms.h"

namespace lime {

/// SDRDevice can have multiple modules (RF chips), that can operate independently

class LIME_API SDRDevice : public IComms
{
  public:
    static constexpr uint8_t MAX_CHANNEL_COUNT = 16;
    static constexpr uint8_t MAX_RFSOC_COUNT = 16;

    enum LogLevel {
        CRITICAL = 0,
        ERROR,
        WARNING,
        INFO,
        VERBOSE,
        DEBUG
    };
    typedef void(*DataCallbackType)(bool, const uint8_t*, const uint32_t);
    typedef void(*LogCallbackType)(LogLevel, const char*);

    enum ClockID
    {
        CLK_REFERENCE = 0,
        CLK_SXR = 1, ///RX LO clock
        CLK_SXT = 2, ///TX LO clock
        CLK_CGEN = 3,
        ///RXTSP reference clock (read-only)
        CLK_RXTSP = 4,
        ///TXTSP reference clock (read-only)
        CLK_TXTSP = 5
    };

    typedef std::unordered_map<std::string, uint32_t> SlaveNameIds_t;

    struct RFSOCDescripion
    {
        std::string name;
        uint8_t channelCount;
        std::vector<std::string> rxPathNames;
        std::vector<std::string> txPathNames;
    };

    // General information about device internals, static capabilities
    struct Descriptor
    {
        std::string name; /// The displayable name for the device
        /*! The displayable name for the expansion card
        * Ex: if the RFIC is on a daughter-card
        */
        std::string expansionName;
        std::string firmwareVersion; /// The firmware version as a string
        std::string gatewareVersion; /// Gateware version as a string
        std::string gatewareRevision; /// Gateware revision as a string
        std::string gatewareTargetBoard; /// Which board should use this gateware
        std::string hardwareVersion; /// The hardware version as a string
        std::string protocolVersion; /// The protocol version as a string
        uint64_t serialNumber; /// A unique board serial number

        SlaveNameIds_t spiSlaveIds; // names and SPI bus numbers of internal chips
        std::vector<RFSOCDescripion> rfSOC;
    };

    struct StreamStats
    {
        StreamStats() {
            memset(this, 0, sizeof(StreamStats));
        }
        uint64_t timestamp;
        int64_t bytesTransferred;
        int64_t packets;
        float FIFO_filled;
        float dataRate_Bps;
        float txDataRate_Bps;
        uint32_t overrun;
        uint32_t underrun;
        uint32_t loss;
        uint32_t late;
        bool isTx;
    };

    // channels order and data transmission formats setup
    struct StreamConfig
    {
        struct Extras {
            Extras() {
                memset(this, 0, sizeof(Extras));
                usePoll = true;
            };
            bool usePoll;
            uint16_t rxSamplesInPacket;
            uint32_t rxPacketsInBatch;
            uint32_t txMaxPacketsInBatch;
            uint16_t txSamplesInPacket;
        };
        typedef bool (*StatusCallbackFunc)(const StreamStats*, void*);
        enum DataFormat
        {
            I16,
            I12,
            F32,
        };

        StreamConfig(){
            memset(this, 0, sizeof(StreamConfig));
        }

        uint8_t rxCount;
        uint8_t rxChannels[MAX_CHANNEL_COUNT];
        uint8_t txCount;
        uint8_t txChannels[MAX_CHANNEL_COUNT];

        DataFormat format;     // samples format used for Read/Write functions
        DataFormat linkFormat; // samples format used in transport layer Host<->FPGA

        /// memory size to allocate for each channel buffering
        /// Default: 0 - allow to decide internally
        uint32_t bufferSize;

        /// optional: expected sampling rate for data transfer optimizations.
        /// Default: 0 - deicide internally
        float hintSampleRate;
        bool alignPhase; // attempt to do phases alignment between paired channels

        StatusCallbackFunc statusCallback;
        void* userData; // will be supplied to statusCallback
        // TODO: callback for drops and errors

        Extras* extraConfig;
    };

    struct StreamMeta
    {
        int64_t timestamp;
        bool useTimestamp;
        bool flush; // submit data to hardware without waiting for full buffer
    };

    struct GFIRFilter
    {
        double bandwidth;
        bool enabled;
    };

    struct ChannelConfig
    {
        ChannelConfig()
        {
            memset(this, 0, sizeof(ChannelConfig));
        }
        double rxCenterFrequency;
        double txCenterFrequency;
        double rxNCOoffset;
        double txNCOoffset;
        double rxSampleRate;
        double txSampleRate;
        double rxGain;
        double txGain;
        uint8_t rxPath;
        uint8_t txPath;
        double rxLPF;
        double txLPF;
        uint8_t rxOversample;
        uint8_t txOversample;
        GFIRFilter rxGFIR;
        GFIRFilter txGFIR;
        bool rxEnabled;
        bool txEnabled;
        bool rxCalibrate;
        bool txCalibrate;
        bool rxTestSignal;
        bool txTestSignal;

        // TODO:
        // TestSignal
        // NCOs
    };

    struct SDRConfig
    {
        SDRConfig() : referenceClockFreq(0), skipDefaults(false) {};
        double referenceClockFreq;
        ChannelConfig channel[MAX_CHANNEL_COUNT];
        // Loopback setup?
        bool skipDefaults; // skip default values initialization and write on top of current config
    };

public:
    virtual ~SDRDevice(){};

    virtual void Configure(const SDRConfig config, uint8_t moduleIndex) = 0;

    /// Returns SPI slave names and chip select IDs for use with SDRDevice::SPI()
    virtual const Descriptor &GetDescriptor() const = 0;

    virtual int Init() = 0;
    virtual void Reset() = 0;

    virtual double GetClockFreq(uint8_t clk_id, uint8_t channel) = 0;
    virtual void SetClockFreq(uint8_t clk_id, double freq, uint8_t channel) = 0;

    virtual void Synchronize(bool toChip) = 0;
    virtual void EnableCache(bool enable) = 0;

    virtual int StreamSetup(const StreamConfig &config, uint8_t moduleIndex) = 0;
    virtual void StreamStart(uint8_t moduleIndex) = 0;
    virtual void StreamStop(uint8_t moduleIndex)= 0;

    virtual int StreamRx(uint8_t channel, void **samples, uint32_t count, StreamMeta *meta) = 0;
    virtual int StreamTx(uint8_t channel, const void **samples, uint32_t count, const StreamMeta *meta) = 0;
    virtual void StreamStatus(uint8_t channel, SDRDevice::StreamStats &status) = 0;


    /***********************************************************************
     * GPIO API
     **********************************************************************/

    /**    @brief Writes GPIO values to device
    @param buffer for source of GPIO values LSB first, each bit sets GPIO state
    @param bufLength buffer length
    @return the operation success state
    */
    virtual int GPIOWrite(const uint8_t *buffer, const size_t bufLength) { return -1;};

    /**    @brief Reads GPIO values from device
    @param buffer destination for GPIO values LSB first, each bit represent GPIO state
    @param bufLength buffer length to read
    @return the operation success state
    */
    virtual int GPIORead(uint8_t *buffer, const size_t bufLength) { return -1;};

    /**    @brief Write GPIO direction control values to device.
    @param buffer with GPIO direction configuration (0 input, 1 output)
    @param bufLength buffer length
    @return the operation success state
    */
    virtual int GPIODirWrite(const uint8_t *buffer, const size_t bufLength) { return -1;};

    /**    @brief Read GPIO direction configuration from device
    @param buffer to put GPIO direction configuration (0 input, 1 output)
    @param bufLength buffer length to read
    @return the operation success state
    */
    virtual int GPIODirRead(uint8_t *buffer, const size_t bufLength) { return -1;};

    /***********************************************************************
     * Aribtrary settings API
     **********************************************************************/

    /** @brief Sets custom on board control to given value units
    @param ids indexes of selected controls
    @param values new control values
    @param count number of values to write
    @param units (optional) when not null specifies value units (e.g V, A, Ohm, C... )
    @return the operation success state
    */
    virtual int CustomParameterWrite(const uint8_t *ids, const double *values, const size_t count, const std::string& units) { return -1;};

    /** @brief Returns value of custom on board control
    @param ids indexes of controls to read
    @param values retrieved control values
    @param count number of values to read
    @param units (optional) when not null returns value units (e.g V, A, Ohm, C... )
    @return the operation success state
    */
    virtual int CustomParameterRead(const uint8_t *ids, double *values, const size_t count, std::string* units) { return -1;};

    /// @brief Sets callback function which gets called each time data is sent or received
    virtual void SetDataLogCallback(DataCallbackType callback) {};
    virtual void SetMessageLogCallback(LogCallbackType callback) {};

    virtual void *GetInternalChip(uint32_t index) { return nullptr; };
    virtual void SetFPGAInterfaceFreq(uint8_t interp, uint8_t dec, double txPhase, double rxPhase) = 0;
};

}
#endif
