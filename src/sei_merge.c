#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>

#include "sei_merge.h"

// Structure to represent a UUID (only defined here, not in header)
typedef struct {
    uint32_t time_low;                  // 32 bits
    uint16_t time_mid;                  // 16 bits
    uint16_t time_hi_and_version;       // 16 bits
    uint8_t  clock_seq_hi_and_reserved; // 8 bits
    uint8_t  clock_seq_low;             // 8 bits
    uint8_t  node[6];                   // 48 bits
} uuid_t;

/*
 * Generates cryptographically secure random bytes.
 * @param buffer Pointer to the buffer to be filled.
 * @param size Number of bytes to generate.
 * @return 0 on success, -1 on error.
 */
static int generate_random_bytes(void *buffer, size_t size) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        // Fallback vers /dev/random si /dev/urandom n'est pas disponible
        fd = open("/dev/random", O_RDONLY);
        if (fd < 0) {
            return -1;
        }
    }
    
    ssize_t bytes_read = read(fd, buffer, size);
    close(fd);
    
    return (bytes_read == (ssize_t)size) ? 0 : -1;
}

/**
 * Generates pseudo-random bytes (fallback)
 * @param buffer Pointer to the buffer to be filled
 * @param size Number of bytes to generate
 */
static void generate_pseudo_random_bytes(void *buffer, size_t size) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)(time(NULL) ^ getpid()));
        seeded = 1;
    }
    
    uint8_t *buf = (uint8_t *)buffer;
    for (size_t i = 0; i < size; i++) {
        buf[i] = (uint8_t)(rand() % 256);
    }
}

/**
 * Generates a UUID v4 compliant with ISO/IEC 11578
 * @param uuid Pointer to the uuid_t structure to be filled
 * @return 0 on success, -1 on error
 */
static int generate_uuid_v4(uuid_t *uuid) {
    if (!uuid) {
        return -1;
    }
    
    // Generate 16 random bytes
    uint8_t random_data[16];
    if (generate_random_bytes(random_data, 16) != 0) {
        // Fallback to pseudo-random generation
        generate_pseudo_random_bytes(random_data, 16);
    }
    
    // Fill in the UUID structure
    uuid->time_low = (uint32_t)random_data[0] << 24 | 
                     (uint32_t)random_data[1] << 16 | 
                     (uint32_t)random_data[2] << 8  | 
                     (uint32_t)random_data[3];
    
    uuid->time_mid = (uint16_t)random_data[4] << 8 | 
                     (uint16_t)random_data[5];
    
    uuid->time_hi_and_version = (uint16_t)random_data[6] << 8 | 
                                (uint16_t)random_data[7];
    
    uuid->clock_seq_hi_and_reserved = random_data[8];
    uuid->clock_seq_low = random_data[9];
    
    // Copy the 6 bytes of the node
    memcpy(uuid->node, &random_data[10], 6);
    
    // Set version 4 (bits 12-15 of time_hi_and_version)
    uuid->time_hi_and_version &= 0x0FFF;
    uuid->time_hi_and_version |= 0x4000;
    
    // Define the RFC 4122 variant (bits 6-7 of clock_seq_hi_and_reserved)
    uuid->clock_seq_hi_and_reserved &= 0x3F;
    uuid->clock_seq_hi_and_reserved |= 0x80;
    
    return 0;
}

/**
 * Converts a UUID to a character string without dashes
 * @param uuid Pointer to the uuid_t structure
 * @param str Output buffer (minimum 33 characters)
 */
static void uuid_to_string(const uuid_t *uuid, char *str) {
    snprintf(str, 33, "%08x%04x%04x%02x%02x%02x%02x%02x%02x",
             uuid->time_low,
             uuid->time_mid,
             uuid->time_hi_and_version,
             uuid->clock_seq_hi_and_reserved,
             uuid->clock_seq_low,
             uuid->node[0], uuid->node[1], uuid->node[2],
             uuid->node[3], uuid->node[4], uuid->node[5]);
}

static GstBuffer *
create_lcevc_user_data_unregistered_sei(const guint8 *sei_data, gsize sei_size, GstLvCompositorCodec codec_type)
{
    GstBuffer *sei_buffer;
    GstMapInfo map;
    guint8 *data;
    gsize pos = 0;
    
    // Payload = UUID (16 bytes) + LCEVC data
    gsize payload_size = 16 + sei_size;
    
    // Calculate size field bytes (same for all codecs)
    gsize size_bytes = 0;
    gsize temp_size = payload_size;
    do {
        size_bytes++;
        temp_size = (temp_size >= 255) ? (temp_size - 255) : 0;
    } while (temp_size > 0);
    
    // Calculate total size based on codec
    gsize total_size;
    switch (codec_type) {
        case CODEC_H264:
            // H.264: start_code(4) + nal_header(1) + type(1) + size(N) + payload + rbsp_trailing(1)
            total_size = 4 + 1 + 1 + size_bytes + payload_size + 1;
            break;
        case CODEC_H265:
            // HEVC: start_code(4) + nal_header(2) + type(1) + size(N) + payload + rbsp_trailing(1)
            total_size = 4 + 2 + 1 + size_bytes + payload_size + 1;
            break;
        case CODEC_H266:
            // H.266/VVC: start_code(4) + nal_unit_header(2) + type(1) + size(N) + payload + rbsp_trailing(1)
            total_size = 4 + 2 + 1 + size_bytes + payload_size + 1;
            break;
        case CODEC_EVC:
            // EVC: similar to H.266
            total_size = 4 + 2 + 1 + size_bytes + payload_size + 1;
            break;
        default:
            GST_ERROR("Unsupported codec type for SEI creation");
            return NULL;
    }
    
    sei_buffer = gst_buffer_new_allocate(NULL, total_size, NULL);
    if (!sei_buffer) {
        GST_ERROR("Failed to allocate SEI buffer");
        return NULL;
    }
    
    if (!gst_buffer_map(sei_buffer, &map, GST_MAP_WRITE)) {
        GST_ERROR("Failed to map SEI buffer");
        gst_buffer_unref(sei_buffer);
        return NULL;
    }
    
    data = map.data;
    
    // 1. Start code (4 bytes Annex B) - same for all
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x01;
    
    switch (codec_type) {
        case CODEC_H264:
            // 2. H.264 NAL unit header (1 byte)
            // forbidden_zero_bit(1)=0, nal_ref_idc(2)=0, nal_unit_type(5)=6
            data[pos++] = 0x06;  // SEI NAL unit type
            
            // 3. SEI payload type = 5 (user_data_unregistered)
            data[pos++] = 5;
            break;
            
        case CODEC_H265:
            // 2. HEVC NAL unit header (2 bytes)
            // forbidden_zero_bit(1)=0, nal_unit_type(6)=39 or 40, nuh_layer_id(6)=0, nuh_temporal_id_plus1(3)=1
            data[pos++] = 0x4E;  // 0100 1110 - nal_unit_type=39 (prefix SEI)
            data[pos++] = 0x01;  // 0000 0001 - nuh_layer_id=0, temporal_id=0
            
            // 3. HEVC SEI payload type = 5 (user_data_unregistered)
            data[pos++] = 5;
            break;
            
        case CODEC_H266:
            // 2. H.266/VVC NAL unit header (2 bytes)
            // forbidden_zero_bit(1)=0, nuh_reserved_zero_1bit(1)=0, nal_unit_type(5), nuh_layer_id(6), nuh_temporal_id_plus1(3)
            // SEI NAL unit types in VVC:
            // - 21: Prefix SEI NAL unit
            // - 22: Suffix SEI NAL unit
            data[pos++] = 0x55;  // 0101 0101 - nal_unit_type=21 (prefix SEI), nuh_layer_id=0
            data[pos++] = 0x01;  // 0000 0001 - nuh_temporal_id_plus1=1
            
            // 3. VVC SEI payload type = 5 (user_data_unregistered)
            data[pos++] = 5;
            break;
            
        case CODEC_EVC:
            // 2. EVC NAL unit header (similar to HEVC/VVC)
            // Using HEVC-like structure for EVC (check EVC spec for exact values)
            data[pos++] = 0x4E;  // Placeholder - adjust based on EVC spec
            data[pos++] = 0x01;  // Placeholder
            
            // 3. EVC SEI payload type = 5 (user_data_unregistered)
            data[pos++] = 5;
            break;
            
        default:
            gst_buffer_unmap(sei_buffer, &map);
            gst_buffer_unref(sei_buffer);
            return NULL;
    }
    
    // 4. SEI payload size (ff_byte encoding) - same for all codecs
    temp_size = payload_size;
    while (temp_size >= 255) {
        data[pos++] = 0xFF;
        temp_size -= 255;
    }
    data[pos++] = (guint8)temp_size;
    
    // 5. Generate and insert UUID
    uuid_t uuid;
    char uuid_str[33];
    
    if (generate_uuid_v4(&uuid) == 0) {
        uuid_to_string(&uuid, uuid_str);
        memcpy(data + pos, uuid_str, 16);
    } else {
        // Fallback: use zeros if UUID generation fails
        memset(data + pos, 0, 16);
    }
    pos += 16;
    
    // 6. User data payload (LCEVC enhancement data)
    if (sei_data && sei_size > 0) {
        memcpy(data + pos, sei_data, sei_size);
        pos += sei_size;
    }
    
    // 7. RBSP trailing bits - same for all codecs
    data[pos++] = 0x80;
    
    gst_buffer_unmap(sei_buffer, &map);
    gst_buffer_set_size(sei_buffer, pos);
    
    const gchar *codec_name = "UNKNOWN";
    switch (codec_type) {
        case CODEC_H264: codec_name = "H264"; break;
        case CODEC_H265: codec_name = "H265"; break;
        case CODEC_H266: codec_name = "H266"; break;
        case CODEC_EVC: codec_name = "EVC"; break;
        default: break;
    }
    
    GST_DEBUG("Created %s user_data_unregistered SEI: UUID + %zu bytes data, total %zu bytes", 
              codec_name, sei_size, pos);
    
    return sei_buffer;
}

static GstBuffer *
combine_buffers_with_sei(GstBuffer *main_buffer, GstBuffer *sei_buffer, GstLvCompositorCodec codec_type)
{
    (void)(codec_type); // Mark parameter as unused to avoid compiler warnings
    
    if (!main_buffer || !sei_buffer) {
        return NULL;
    }
    
    // For simplicity, just return the SEI buffer as the result
    // In a real implementation, you would merge the SEI NAL unit with the main buffer's NAL units
    GstBuffer *result = gst_buffer_copy(sei_buffer);
    
    // Copy timestamps and other metadata from main buffer
    GST_BUFFER_PTS(result) = GST_BUFFER_PTS(main_buffer);
    GST_BUFFER_DTS(result) = GST_BUFFER_DTS(main_buffer);
    GST_BUFFER_DURATION(result) = GST_BUFFER_DURATION(main_buffer);
    
    // Copy other metadata as needed
    gst_buffer_copy_into(result, main_buffer, GST_BUFFER_COPY_METADATA, 0, -1);
    
    gst_buffer_unref(sei_buffer);
    
    return result;
}

// Public functions that match the declarations in sei_merge.h
GstBuffer *
merge_lcevc_data_h264(GstBuffer *main_buffer, GstBuffer *secondary_buffer)
{
    GstMapInfo secondary_map;
    guint8 *sei_data = NULL;
    gsize sei_size = 0;
    
    if (!gst_buffer_map(secondary_buffer, &secondary_map, GST_MAP_READ)) {
        GST_ERROR("Failed to map secondary buffer for H.264");
        return NULL;
    }
    
    sei_data = secondary_map.data;
    sei_size = secondary_map.size;
    
    GstBuffer *sei_buffer = create_lcevc_user_data_unregistered_sei(sei_data, sei_size, CODEC_H264);
    
    gst_buffer_unmap(secondary_buffer, &secondary_map);
    
    if (!sei_buffer) {
        GST_ERROR("Failed to create H.264 SEI buffer");
        return NULL;
    }
    
    // Combine main buffer with SEI buffer
    return combine_buffers_with_sei(main_buffer, sei_buffer, CODEC_H264);
}

GstBuffer *
merge_lcevc_data_h265(GstBuffer *main_buffer, GstBuffer *secondary_buffer)
{
    GstMapInfo secondary_map;
    guint8 *sei_data = NULL;
    gsize sei_size = 0;
    
    if (!gst_buffer_map(secondary_buffer, &secondary_map, GST_MAP_READ)) {
        GST_ERROR("Failed to map secondary buffer for H.265");
        return NULL;
    }
    
    sei_data = secondary_map.data;
    sei_size = secondary_map.size;
    
    GstBuffer *sei_buffer = create_lcevc_user_data_unregistered_sei(sei_data, sei_size, CODEC_H265);
    
    gst_buffer_unmap(secondary_buffer, &secondary_map);
    
    if (!sei_buffer) {
        GST_ERROR("Failed to create H.265 SEI buffer");
        return NULL;
    }
    
    // Combine main buffer with SEI buffer
    return combine_buffers_with_sei(main_buffer, sei_buffer, CODEC_H265);
}

GstBuffer *
merge_lcevc_data_h266(GstBuffer *main_buffer, GstBuffer *secondary_buffer)
{
    GstMapInfo secondary_map;
    guint8 *sei_data = NULL;
    gsize sei_size = 0;
    
    if (!gst_buffer_map(secondary_buffer, &secondary_map, GST_MAP_READ)) {
        GST_ERROR("Failed to map secondary buffer for H.266");
        return NULL;
    }
    
    sei_data = secondary_map.data;
    sei_size = secondary_map.size;
    
    GstBuffer *sei_buffer = create_lcevc_user_data_unregistered_sei(sei_data, sei_size, CODEC_H266);
    
    gst_buffer_unmap(secondary_buffer, &secondary_map);
    
    if (!sei_buffer) {
        GST_ERROR("Failed to create H.266 SEI buffer");
        return NULL;
    }
    
    // Combine main buffer with SEI buffer
    return combine_buffers_with_sei(main_buffer, sei_buffer, CODEC_H266);
}

GstBuffer *
merge_lcevc_data_evc(GstBuffer *main_buffer, GstBuffer *secondary_buffer)
{
    GstMapInfo secondary_map;
    guint8 *sei_data = NULL;
    gsize sei_size = 0;
    
    if (!gst_buffer_map(secondary_buffer, &secondary_map, GST_MAP_READ)) {
        GST_ERROR("Failed to map secondary buffer for EVC");
        return NULL;
    }
    
    sei_data = secondary_map.data;
    sei_size = secondary_map.size;
    
    GstBuffer *sei_buffer = create_lcevc_user_data_unregistered_sei(sei_data, sei_size, CODEC_EVC);
    
    gst_buffer_unmap(secondary_buffer, &secondary_map);
    
    if (!sei_buffer) {
        GST_ERROR("Failed to create EVC SEI buffer");
        return NULL;
    }
    
    // Combine main buffer with SEI buffer
    return combine_buffers_with_sei(main_buffer, sei_buffer, CODEC_EVC);
}

GstBuffer *
merge_lcevc_data_generic(GstBuffer *main_buffer, GstBuffer *secondary_buffer)
{
    // Fallback to H.265 for generic case
    return merge_lcevc_data_h265(main_buffer, secondary_buffer);
}