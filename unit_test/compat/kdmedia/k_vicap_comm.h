#ifndef UNIT_TEST_K_VICAP_COMM_H
#define UNIT_TEST_K_VICAP_COMM_H

#include "k_types.h"

enum k_vicap_sensor_type {
    SENSOR_TYPE_DUMMY = 0,
    SENSOR_TYPE_MAX = 1024,
};

typedef enum {
    VICAP_MIPI_LANE_PREF_ANY = 0,
    VICAP_MIPI_LANE_PREF_2LANE = 1,
    VICAP_MIPI_LANE_PREF_4LANE = 2,
} k_vicap_mipi_lane_pref;

#endif
