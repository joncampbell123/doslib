
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

const char *omf_fixupp_location_to_str(const unsigned char loc) {
    switch (loc) {
        case OMF_FIXUPP_LOCATION_LOBYTE:                return "LOBYTE";
        case OMF_FIXUPP_LOCATION_16BIT_OFFSET:          return "16BIT-OFFSET";
        case OMF_FIXUPP_LOCATION_16BIT_SEGMENT_BASE:    return "16BIT-SEGBASE";
        case OMF_FIXUPP_LOCATION_16BIT_SEGMENT_OFFSET:  return "16BIT-SEG-OFFSET";
        case OMF_FIXUPP_LOCATION_HIBYTE:                return "HIBYTE";
        case OMF_FIXUPP_LOCATION_16BIT_OFFSET_RESOLVED: return "16BIT-OFFSET-RESOLVED";
        case OMF_FIXUPP_LOCATION_32BIT_OFFSET:          return "32BIT-OFFSET";
        case OMF_FIXUPP_LOCATION_32BIT_SEGMENT_OFFSET:  return "32BIT-SEG-OFFSET";
        case OMF_FIXUPP_LOCATION_32BIT_OFFSET_RESOLVED: return "32BIT-OFFSET-RESOLVED";
    };

    return "?";
}

const char *omf_fixupp_frame_method_to_str(const unsigned char m) {
    switch (m) {
        case OMF_FIXUPP_FRAME_METHOD_SEGDEF:            return "SEGDEF";
        case OMF_FIXUPP_FRAME_METHOD_GRPDEF:            return "GRPDEF";
        case OMF_FIXUPP_FRAME_METHOD_EXTDEF:            return "EXTDEF";
        case OMF_FIXUPP_FRAME_METHOD_PREV_LEDATA:       return "by-LEDATA-segment";
        case OMF_FIXUPP_FRAME_METHOD_TARGET:            return "by-TARGET";
    };

    return "?";
}

const char *omf_fixupp_target_method_to_str(const unsigned char m) {
    switch (m) {
        case OMF_FIXUPP_TARGET_METHOD_SEGDEF:           return "SEGDEF";
        case OMF_FIXUPP_TARGET_METHOD_GRPDEF:           return "GRPDEF";
        case OMF_FIXUPP_TARGET_METHOD_EXTDEF:           return "EXTDEF";
    };

    return "?";
}

