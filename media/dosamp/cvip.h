
uint32_t convert_ip_8_to_16(const uint32_t total_samples,void dosamp_FAR * const proc_buf,const uint32_t buf_max);
uint32_t convert_ip_16_to_8(const uint32_t total_samples,void dosamp_FAR * const proc_buf,const uint32_t buf_max);

uint32_t convert_rdbuf_stereo2mono_ip_u8(uint32_t samples,void dosamp_FAR * const proc_buf,const uint32_t buf_max);
uint32_t convert_rdbuf_stereo2mono_ip_s16(uint32_t samples,void dosamp_FAR * const proc_buf,const uint32_t buf_max);

