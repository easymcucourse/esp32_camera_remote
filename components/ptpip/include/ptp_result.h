#pragma once
/* REFUSED is synchronized; IO/PROTOCOL require closing the transport. */
typedef enum { PTP_DATA_OK, PTP_DATA_REFUSED, PTP_DATA_IO, PTP_DATA_PROTOCOL } ptp_data_status_t;
