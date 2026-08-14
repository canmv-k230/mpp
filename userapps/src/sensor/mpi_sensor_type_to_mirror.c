#include <stdio.h>

#include "k_sensor_comm.h"

#define MIRROR  (1)
#define FLIP    (2)

struct sensor_type_mirror_t {
  k_vicap_sensor_type type;
  k_u32 mirror;
};

#if defined(CONFIG_BOARD_K230_CANMV)
static struct sensor_type_mirror_t type_mirror_tbl[] = {
#if defined (CONFIG_MPP_ENABLE_SENSOR_OV5647)
    {.type = OV5647_MIPI_CSI0_2592x1944_15FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI0_1920X1080_30FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI0_1280X960_45FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI0_1280X720_60FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI0_640x480_90FPS_10BIT_LINEAR, .mirror = 0},

    {.type = OV5647_MIPI_CSI1_2592x1944_15FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI1_1920X1080_30FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI1_1280X960_45FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI1_1280X720_60FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI1_640x480_90FPS_10BIT_LINEAR, .mirror = 0},

    {.type = OV5647_MIPI_CSI2_2592x1944_15FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI2_1920X1080_30FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI2_1280X960_45FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI2_1280X720_60FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI2_640x480_90FPS_10BIT_LINEAR, .mirror = 0},
#endif // CONFIG_MPP_ENABLE_SENSOR_OV5647

#if defined (CONFIG_MPP_ENABLE_SENSOR_GC2093)
    {.type = GC2093_MIPI_CSI0_1920X1080_30FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI0_1920X1080_60FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI0_1280X960_90FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI0_1280X720_90FPS_10BIT_LINEAR, .mirror = 0},

    {.type = GC2093_MIPI_CSI1_1920X1080_30FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI1_1920X1080_60FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI1_1280X960_90FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI1_1280X720_90FPS_10BIT_LINEAR, .mirror = 0},

    {.type = GC2093_MIPI_CSI2_1920X1080_30FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI2_1920X1080_60FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI2_1280X960_90FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI2_1280X720_90FPS_10BIT_LINEAR, .mirror = 0},
#endif // CONFIG_MPP_ENABLE_SENSOR_GC2093
};
#elif defined(CONFIG_BOARD_K230_CANMV_V2)
static struct sensor_type_mirror_t type_mirror_tbl[] = {
    // TODO: Add sensor type mirror configurations for K230_CANMV_V2
    { .type = 0, .mirror = 0 },  /* Dummy entry to avoid empty array */
};
#elif defined(CONFIG_BOARD_K230D_CANMV)
static struct sensor_type_mirror_t type_mirror_tbl[] = {
    // TODO: Add sensor type mirror configurations for K230D_CANMV
    { .type = 0, .mirror = 0 },  /* Dummy entry to avoid empty array */
};
#elif defined(CONFIG_BOARD_K230_CANMV_01STUDIO)
static struct sensor_type_mirror_t type_mirror_tbl[] = {
#if defined (CONFIG_MPP_ENABLE_SENSOR_OV5647)
    {.type = OV5647_MIPI_CSI0_2592x1944_15FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI0_1920X1080_30FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI0_1280X960_45FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI0_1280X720_60FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI0_640x480_90FPS_10BIT_LINEAR, .mirror = 0},

    {.type = OV5647_MIPI_CSI1_2592x1944_15FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI1_1920X1080_30FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI1_1280X960_45FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI1_1280X720_60FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI1_640x480_90FPS_10BIT_LINEAR, .mirror = 0},

    {.type = OV5647_MIPI_CSI2_2592x1944_15FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI2_1920X1080_30FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI2_1280X960_45FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI2_1280X720_60FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV5647_MIPI_CSI2_640x480_90FPS_10BIT_LINEAR, .mirror = 0},
#endif // CONFIG_MPP_ENABLE_SENSOR_OV5647

#if defined (CONFIG_MPP_ENABLE_SENSOR_GC2093)
    {.type = GC2093_MIPI_CSI0_1920X1080_30FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI0_1920X1080_60FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI0_1280X960_90FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI0_1280X720_90FPS_10BIT_LINEAR, .mirror = 0},

    {.type = GC2093_MIPI_CSI1_1920X1080_30FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI1_1920X1080_60FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI1_1280X960_90FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI1_1280X720_90FPS_10BIT_LINEAR, .mirror = 0},

    {.type = GC2093_MIPI_CSI2_1920X1080_30FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI2_1920X1080_60FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI2_1280X960_90FPS_10BIT_LINEAR, .mirror = 0},
    {.type = GC2093_MIPI_CSI2_1280X720_90FPS_10BIT_LINEAR, .mirror = 0},
#endif // CONFIG_MPP_ENABLE_SENSOR_GC2093

#if defined (CONFIG_MPP_ENABLE_SENSOR_OV13850)
    {.type = OV13850_MIPI_CSI0_3840x2160_7FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV13850_MIPI_CSI1_3840x2160_7FPS_10BIT_LINEAR, .mirror = 0},
    {.type = OV13850_MIPI_CSI2_3840x2160_7FPS_10BIT_LINEAR, .mirror = 0},
#endif // CONFIG_MPP_ENABLE_SENSOR_OV13850
#if defined (CONFIG_MPP_ENABLE_SENSOR_IMX675)
    /* Module default orientation uses H+V reverse; NONE in driver maps to that. */
    {.type = IMX675_MIPI_CSI0_2LANE_RAW10_2592X1944_30FPS_LINEAR, .mirror = 0},
    {.type = IMX675_MIPI_CSI1_2LANE_RAW10_2592X1944_30FPS_LINEAR, .mirror = 0},
    {.type = IMX675_MIPI_CSI2_2LANE_RAW10_2592X1944_30FPS_LINEAR, .mirror = 0},
#endif // CONFIG_MPP_ENABLE_SENSOR_IMX675
#if defined (CONFIG_MPP_ENABLE_SENSOR_SC130GS)
    {.type = SC130GS_MIPI_CSI0_2LANE_1280X1024_90FPS_10BIT_LINEAR, .mirror = 0},
    {.type = SC130GS_MIPI_CSI1_2LANE_1280X1024_90FPS_10BIT_LINEAR, .mirror = 0},
    {.type = SC130GS_MIPI_CSI2_2LANE_1280X1024_90FPS_10BIT_LINEAR, .mirror = 0},
#endif // CONFIG_MPP_ENABLE_SENSOR_SC130GS
};
#elif defined(CONFIG_BOARD_K230_CANMV_DONGSHANPI)
static struct sensor_type_mirror_t type_mirror_tbl[] = {
    // TODO: Add sensor type mirror configurations for K230_CANMV_DONGSHANPI
    { .type = 0, .mirror = 0 },  /* Dummy entry to avoid empty array */
};
#elif defined(CONFIG_BOARD_K230_CANMV_RTT_EVB)
static struct sensor_type_mirror_t type_mirror_tbl[] = {
    // TODO: Add sensor type mirror configurations for K230_CANMV_RTT_EVB
    { .type = 0, .mirror = 0 },  /* Dummy entry to avoid empty array */
};

#elif defined(CONFIG_BOARD_K230D_CANMV_BPI_ZERO)
static struct sensor_type_mirror_t type_mirror_tbl[] = {
    // TODO: Add sensor type mirror configurations for K230D_CANMV_BPI_ZERO
    { .type = 0, .mirror = 0 },  /* Dummy entry to avoid empty array */
};
#elif defined(CONFIG_BOARD_K230D_CANMV_ATK_DNK230D)
static struct sensor_type_mirror_t type_mirror_tbl[] = {
    // TODO: Add sensor type mirror configurations for K230D_CANMV_ATK_DNK230D
    { .type = 0, .mirror = 0 },  /* Dummy entry to avoid empty array */
};
#endif

#if defined(CONFIG_BOARD_K230_CANMV) || defined(CONFIG_BOARD_K230_CANMV_V2) || \
    defined(CONFIG_BOARD_K230D_CANMV) ||                                       \
    defined(CONFIG_BOARD_K230_CANMV_01STUDIO) ||                               \
    defined(CONFIG_BOARD_K230_CANMV_DONGSHANPI) || \
    defined(CONFIG_BOARD_K230_CANMV_RTT_EVB) || \
    defined(CONFIG_BOARD_K230D_CANMV_BPI_ZERO) || \
    defined(CONFIG_BOARD_K230D_CANMV_ATK_DNK230D)

k_u32 get_mirror_by_sensor_type(k_vicap_sensor_type type) {
  k_u32 mirror = 0;

  struct sensor_type_mirror_t *p = NULL;

  size_t count = (sizeof(type_mirror_tbl) / sizeof(type_mirror_tbl[0]));

  for (size_t i = 0; i < count; i++) {
    p = &type_mirror_tbl[i];

    if (type == p->type) {
      mirror = p->mirror;
      break;
    }
  }

  return mirror;
}
#else
k_u32 get_mirror_by_sensor_type(k_vicap_sensor_type type) {
  printf("board %s use default sensor mirror setting\n", CONFIG_BOARD_NAME);

  return 0;
}
#endif
