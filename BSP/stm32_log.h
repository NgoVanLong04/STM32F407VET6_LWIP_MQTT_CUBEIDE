#ifndef STM32_LOG_H
#define STM32_LOG_H

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- Cau hinh (co the override bang -D hoac define truoc khi include) ---------------- */

/* Do dai toi da 1 dong log (gom ca prefix va "\r\n") */
#ifndef MAX_LEN_PRINT_BUFFER
#define MAX_LEN_PRINT_BUFFER	256
#endif

/* So slot cua hang doi. Dung duoc toi da (N - 1) log cung luc */
#ifndef LOG_QUEUE_BUFFER_SIZE
#define LOG_QUEUE_BUFFER_SIZE	64
#endif

/* Thoi gian toi da (theo don vi cua LOG_GET_TIME) moi lan goi log_handler(). 0 = khong gioi han */
#ifndef LOG_HANDLER_MAX_TIME_MS
#define LOG_HANDLER_MAX_TIME_MS	0
#endif

/* Backend async (DMA/USB) qua thoi gian nay ma chua goi log_tx_done() thi coi nhu that bai va bo dong do.
 * Tranh ket vinh vien khi vd khong cam USB / khong mo terminal. 0 = khong timeout */
#ifndef LOG_TX_TIMEOUT_MS
#define LOG_TX_TIMEOUT_MS		100
#endif

/* Muc log mac dinh luc khoi dong */
#ifndef LOG_DEFAULT_LEVEL
#define LOG_DEFAULT_LEVEL		LOG_INFO
#endif

/* Muc log toi da duoc bien dich vao (log muc cao hon se bi compiler loai bo) */
#ifndef LOG_COMPILE_LEVEL
#define LOG_COMPILE_LEVEL		LOG_VERBOSE
#endif

/*
 * Cac hook co the override (xem stm32_log.c):
 *   LOG_GET_TIME()           -> uint32_t, mac dinh HAL_GetTick()
 *   LOG_LOCK_ENTER(state)    -> vao critical section, luu trang thai vao `state` (uint32_t)
 *   LOG_LOCK_EXIT(state)     -> thoat critical section
 */

/* ---------------- Kieu du lieu ---------------- */

/* Muc log: so cang nho cang nghiem trong (giong ESP-IDF) */
typedef enum log_level_t {
    LOG_ERROR = 0,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG,
    LOG_VERBOSE
} log_level_t;

/* Ma loi */
typedef enum log_err_t {
    LOG_OK = 0,
    LOG_OVERFLOW_BUFFER,	/* dong log bi cat bot (van duoc gui) */
    LOG_OVERFLOW_QUEUE,		/* hang doi day, log bi bo */
    LOG_INVALID,
    LOG_TX_TIMEOUT			/* backend async khong bao xong trong LOG_TX_TIMEOUT_MS, log bi bo */
} log_err_t;

/* Ket qua tra ve cua backend */
typedef enum log_port_ret_t {
    LOG_PORT_BUSY = 0,		/* backend ban, chua nhan, se thu lai o lan sau */
    LOG_PORT_DONE,			/* da gui xong (hoac da copy xong), giai phong slot ngay */
    LOG_PORT_ASYNC			/* da bat dau gui (DMA/IT), slot van duoc giu den khi goi log_tx_done() */
} log_port_ret_t;

/* ---------------- API ---------------- */

log_level_t get_log_level(void);
void set_log_level(log_level_t level);
log_err_t get_last_err(void);
uint32_t get_err_count(void);		/* doc xong se xoa ve 0 */

/* Day 1 dong log vao hang doi (khong block). Dung qua LOGE/LOGW/LOGI/LOGD/LOGV */
log_err_t add_log_queue(log_level_t level, const char *tag, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
log_err_t log_vadd_queue(log_level_t level, const char *tag, const char *fmt, va_list ap);

/* Goi trong main loop / task rieng. Xa log ra backend, khong block lau */
log_err_t log_handler(void);

/* Backend: ghi 1 dong log ra ngoai (UART...). Ban weak mac dinh goi _write() (blocking).
 * Hay override bang ban DMA/IT. */
log_port_ret_t log_port_write(const char *data, uint32_t len);

/* Neu backend tra ve LOG_PORT_ASYNC, goi ham nay khi truyen xong (vd HAL_UART_TxCpltCallback) */
void log_tx_done(void);

/* Chi goi khi he thong dang khong log */
void reset_log_queue(void);

/* ---------------- Macro kieu ESP-IDF ---------------- */

#define LOG_FMT_(level, tag, fmt, ...) \
    do { \
        if ((LOG_COMPILE_LEVEL) >= (level)) { \
            (void)add_log_queue((level), (tag), (fmt), ##__VA_ARGS__); \
        } \
    } while (0)

#define LOGE(tag, fmt, ...)	LOG_FMT_(LOG_ERROR,   tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...)	LOG_FMT_(LOG_WARNING, tag, fmt, ##__VA_ARGS__)
#define LOGI(tag, fmt, ...)	LOG_FMT_(LOG_INFO,    tag, fmt, ##__VA_ARGS__)
#define LOGD(tag, fmt, ...)	LOG_FMT_(LOG_DEBUG,   tag, fmt, ##__VA_ARGS__)
#define LOGV(tag, fmt, ...)	LOG_FMT_(LOG_VERBOSE, tag, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* STM32_LOG_H */
