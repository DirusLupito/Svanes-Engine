#pragma once

#include <svanes/network/network_message.hpp>

#include <span>

namespace svanes {

/**
 * Builds a NetworkMessage one field at a time, appending bytes in the order
 * the write functions are called. Multi-byte values use big-endian order,
 * with the most significant byte first, so the format does not depend on
 * the sending machine's byte order or how a C++ struct is laid out in memory.
 * 
 * The message contains only the values written. Field names, types, and byte
 * array lengths are not added automatically. The sender and receiver agree
 * on the layout, and MessageReader reads those fields back in the same order.
 * 
 * Finish() transfers the completed message out and clears the writer for reuse.
 */
class MessageWriter {
public:
    /**
     * Writes an unsigned 8-bit integer.
     * @param value The value to append.
     */
    void WriteUint8(std::uint8_t value);

    /**
     * Writes an unsigned 16-bit integer in big-endian order.
     * @param value The value to append.
     */
    void WriteUint16(std::uint16_t value);

    /**
     * Writes an unsigned 32-bit integer in big-endian order.
     * @param value The value to append.
     */
    void WriteUint32(std::uint32_t value);

    /**
     * Writes an unsigned 64-bit integer in big-endian order.
     * @param value The value to append.
     */
    void WriteUint64(std::uint64_t value);

    /**
     * Writes a signed 32-bit integer using its two's-complement bits.
     * @param value The value to append.
     */
    void WriteInt32(std::int32_t value);

    /**
     * Writes the IEEE-754 bits of a 32-bit float in big-endian order.
     * @param value The value to append.
     */
    void WriteFloat32(float value);

    /**
     * Writes a boolean as one byte containing zero or one.
     * @param value The value to append.
     */
    void WriteBool(bool value);

    /**
     * Appends bytes without adding a length prefix.
     * @param bytes The bytes to copy into the message.
     */
    void WriteBytes(std::span<const std::byte> bytes);

    /**
     * Returns the completed message and resets the writer for reuse.
     * @return The message containing the fields written so far.
     */
    NetworkMessage Finish();

private:
    /**
     * Writes the requested number of bytes, most significant byte first.
     * @param value The unsigned value to encode.
     * @param width The number of bytes to write, from 1 to 8.
     */
    void WriteUnsigned(std::uint64_t value, std::uint32_t width);
    NetworkMessage message;
};

/**
 * Reads the fields of a NetworkMessage in the format produced by MessageWriter.
 * 
 * A cursor tracks the next unread byte, advancing as each field is read and
 * converting big-endian bytes back into the requested value. The reader does
 * not know the message's layout- the caller chooses the matching read functions
 * in the same order the fields were written. Each read checks that the whole
 * field is available and throws std::invalid_argument if it is truncated.
 * 
 * The source bytes are borrowed rather than copied, so they must stay alive
 * and unchanged while reading. Remaining() lets the caller check for unread
 * bytes after decoding the expected fields.
 */
class MessageReader {
public:
    /**
     * Creates a reader over an existing message without copying its bytes.
     * @param message The message to read, kept alive and unchanged while reading.
     */
    explicit MessageReader(const NetworkMessage &message);

    /**
     * Creates a reader over an existing byte span.
     * @param bytes The bytes to read, kept alive and unchanged while reading.
     */
    explicit MessageReader(std::span<const std::byte> bytes);

    /**
     * @return The next unsigned 8-bit integer.
     */
    std::uint8_t ReadUint8();

    /**
     * @return The next unsigned 16-bit integer decoded from big-endian bytes.
     */
    std::uint16_t ReadUint16();

    /**
     * @return The next unsigned 32-bit integer decoded from big-endian bytes.
     */
    std::uint32_t ReadUint32();

    /**
     * @return The next unsigned 64-bit integer decoded from big-endian bytes.
     */
    std::uint64_t ReadUint64();

    /**
     * @return The next signed 32-bit integer decoded from two's-complement bits.
     */
    std::int32_t ReadInt32();

    /**
     * @return The next 32-bit float decoded from its IEEE-754 bits.
     */
    float ReadFloat32();

    /**
     * Reads one byte as a boolean.
     * @return Whether the byte is one.
     * @throws std::invalid_argument if the byte is neither zero nor one.
     */
    bool ReadBool();

    /**
     * Returns the next bytes and advances the cursor without copying them.
     * @param count The number of bytes to read.
     * @return A borrowed span into the source message.
     * @throws std::invalid_argument if fewer than count bytes remain.
     */
    std::span<const std::byte> ReadBytes(std::size_t count);

    /**
     * @return The number of unread bytes in the source message.
     */
    std::size_t Remaining() const;

private:
    /**
     * Decodes the next bytes as an unsigned big-endian integer.
     * @param width The number of bytes to read, from 1 to 8.
     * @return The decoded integer.
     */
    std::uint64_t ReadUnsigned(std::uint32_t width);
    std::span<const std::byte> bytes;
    std::size_t cursor = 0;
};

}
