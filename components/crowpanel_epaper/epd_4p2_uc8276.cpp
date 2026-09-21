#include "crowpanel_epaper.h"
#include "uc8276.h"
#include "esphome/core/log.h"

namespace esphome {
namespace crowpanel_epaper {

static const char *const TAG = "epd_4p2_uc8276";

// Panel setting 0x3F selects LUT-from-register. This panel's OTP holds no
// usable waveform - with 0x1F it never refreshes - so the tables below have to
// be uploaded, and before power-on.
//
// PATCHED: panel setting / booster soft-start / VCOM_DC / VCOM interval below
// were changed from the upstream values (0xCA / 0xE7,0xE7,0x3D / 0x00 / 0xB7)
// to match Elecrow's own official "green circular sticker" v1.2 reference
// driver (EPD_Init() in EPD.cpp), which uses 0x4D / 0x96,0x96,0x29 / 0x05 /
// 0x97. The upstream values under-drive VCOM (0x00 is effectively no bias),
// which produces a pale/washed-out full-black instead of true black.
static const uint8_t SEQ_INIT[] = {
    UC_PANEL_SETTING,   0x02, 0x3F, 0x4D,
    UC_POWER_SETTING,   0x05, 0x03, 0x10, 0x3F, 0x3F, 0x03,
    UC_BOOSTER_SOFT_ST, 0x03, 0x96, 0x96, 0x29,
    UC_TCON_SETTING,    0x01, 0x22,
    UC_VCOM_DC_SETTING, 0x01, 0x05,
    UC_PLL_CONTROL,     0x01, 0x09,              // 50 Hz
    UC_POWER_SAVING,    0x01, 0x88,
    UC_RESOLUTION,      0x04, 0x01, 0x90, 0x01, 0x2C,  // 400 x 300
    UC_VCOM_INTERVAL,   0x01, 0x97,              // white border
    SEQ_END, SEQ_END,
};

// Waveforms from Elecrow's v1.2 demo driver (their "green circular sticker"
// example tree). Keep both sets from the same source: DU assumes the state GC
// leaves behind. One 7-byte phase group each, zero-padded to the register
// length on upload.

// GC - full refresh. WW==BW and WB==BB, so the outgoing pixel is irrelevant.
//
// REVERTED (2026-09): briefly tried GxEPD2_420_SE0420NQ04's 4-phase full
// waveform here, ported by matching register numbers (0x20=VCOM, 0x21=WW,
// 0x22=BW, 0x23=WB, 0x24=BB) 1:1. On real hardware this inverted polarity on
// mixed content: black<->white full-panel fills still looked right, but
// drawing new black content onto a white background came out gray/wrong, and
// whole-page black/white came out swapped. Most likely cause: GxEPD2's own
// writeScreenBuffer doesn't invert bits the way this component's
// send_data_() does (see its comment: "a set bit renders black, the opposite
// of the shared framebuffer convention"), so a same-named register in GxEPD2
// isn't necessarily driving the same physical transition as it does here -
// WW/BB (and BW/WB) most likely need to be swapped relative to a naive
// register-number match, not copied as-is. Back to Elecrow's known-good
// single-phase tables until that's verified rather than guessed again.
static const uint8_t LUT_VCOM_FULL[] = {0x01, 0x14, 0x0A, 0x14, 0x00, 0x01, 0x01};
static const uint8_t LUT_WW_FULL[]   = {0x01, 0x54, 0x0A, 0x94, 0x00, 0x01, 0x01};
static const uint8_t LUT_BW_FULL[]   = {0x01, 0x54, 0x0A, 0x94, 0x00, 0x01, 0x01};
static const uint8_t LUT_WB_FULL[]   = {0x01, 0x94, 0x0A, 0x54, 0x00, 0x01, 0x01};
static const uint8_t LUT_BB_FULL[]   = {0x01, 0x94, 0x0A, 0x54, 0x00, 0x01, 0x01};

// DU - partial refresh. WW==BB is an idle waveform, so only changed pixels move.
//
// EXPERIMENTAL (2026-09): WB's (white->black) byte1 was raised from 0x54
// to 0x94, matching BW (black->white). In Elecrow's original table WB ran
// for roughly half as long as BW (0x54=84 units vs 0x94=148 units) -
// drawing new black text on a white background is exactly this weak WB
// transition, and is the likely reason text came out grayish in partial
// mode. The other registers (VCOM/WW/BB) were left untouched; the
// previous value was 0x54, in case this needs reverting (e.g. if it
// increases ghosting).
static const uint8_t LUT_VCOM_PART[] = {0x01, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00};
static const uint8_t LUT_WW_PART[]   = {0x01, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00};
static const uint8_t LUT_BW_PART[]   = {0x01, 0x94, 0x00, 0x00, 0x00, 0x01, 0x00};
static const uint8_t LUT_WB_PART[]   = {0x01, 0x94, 0x00, 0x00, 0x00, 0x01, 0x00};
static const uint8_t LUT_BB_PART[]   = {0x01, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00};

static const uint8_t SEQ_REFRESH[] = {
    UC_DISPLAY_REFRESH, DELAY_BIT, 10,
    SEQ_END, SEQ_END,
};

static const uint8_t SEQ_SLEEP[] = {
    UC_POWER_OFF,  DELAY_BIT, 100,
    UC_DEEP_SLEEP, 0x01, UC_DEEP_SLEEP_CHECK,
    SEQ_END, SEQ_END,
};

void CrowPanelEPaper4P2InUC8276::send_lut_(uint8_t cmd, const uint8_t *lut, size_t len, size_t total) {
  spi_command_(cmd);
  spi_start_data_();
  for (size_t i = 0; i < total; i++)
    spi_write_byte_(i < len ? lut[i] : 0x00);
  spi_end_data_();
}

void CrowPanelEPaper4P2InUC8276::send_lut_set_(bool full) {
  send_lut_(UC_LUT_VCOM, full ? LUT_VCOM_FULL : LUT_VCOM_PART, UC_LUT_GROUP_LEN, UC_LUT_REG_LEN);
  send_lut_(UC_LUT_WW, full ? LUT_WW_FULL : LUT_WW_PART, UC_LUT_GROUP_LEN, UC_LUT_REG_LEN);
  send_lut_(UC_LUT_BW, full ? LUT_BW_FULL : LUT_BW_PART, UC_LUT_GROUP_LEN, UC_LUT_REG_LEN);
  send_lut_(UC_LUT_WB, full ? LUT_WB_FULL : LUT_WB_PART, UC_LUT_GROUP_LEN, UC_LUT_REG_LEN);
  send_lut_(UC_LUT_BB, full ? LUT_BB_FULL : LUT_BB_PART, UC_LUT_GROUP_LEN, UC_LUT_REG_LEN);
}

void CrowPanelEPaper4P2InUC8276::init_display_() {
  ESP_LOGD(TAG, "Initialising 4.2\" UC8276C display");
  old_plane_primed_ = false;

  spi_send_sequence_(SEQ_INIT);
  send_lut_set_(true);

  // Power on last - INIT_WAIT does the BUSY wait for us.
  spi_command_(UC_POWER_ON);
}

void CrowPanelEPaper4P2InUC8276::prepare_update_() {
  send_lut_set_(is_full_update_);

  // 0x10 holds the previous frame, 0x13 the new one. 0x10 is garbage at
  // power-up so prime it once; the controller maintains it after each refresh.
  if (!old_plane_primed_) {
    spi_command_(UC_DATA_START_1);
    spi_start_data_();
    for (uint32_t i = 0, n = get_buffer_length_(); i < n; i++)
      spi_write_byte_(UC_PIXEL_WHITE);
    spi_end_data_();
    old_plane_primed_ = true;
  }

  spi_command_(UC_DATA_START_2);
  spi_start_data_();
}

void CrowPanelEPaper4P2InUC8276::send_data_() {
  // This controller renders a set bit black, the opposite of the shared
  // framebuffer convention.
  for (uint32_t i = 0, n = get_buffer_length_(); i < n; i++)
    spi_write_byte_(~buffer_[i]);
  spi_end_data_();
}

// Full and partial share one refresh command; the waveform loaded by
// prepare_update_() carries the difference.
const uint8_t *CrowPanelEPaper4P2InUC8276::refresh_seq_(bool /*full*/) const {
  return SEQ_REFRESH;
}

const uint8_t *CrowPanelEPaper4P2InUC8276::sleep_seq_() const { return SEQ_SLEEP; }

}  // namespace crowpanel_epaper
}  // namespace esphome
