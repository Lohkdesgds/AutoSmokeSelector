#include "package_definition.h"

int build_package_raw(struct esp_package* res, const char* in, unsigned long siz, const int typ)
{
	if (siz <= 0) return -1; // invalid size
	if (res == NULL || in == NULL) return -2; // invalid pointer

	res->left = (siz - 1) / PACKAGE_SIZE;
	
	if (siz > PACKAGE_SIZE) siz = PACKAGE_SIZE;

	memcpy(res->data.raw, in, siz);
	res->len = siz;
	res->type = typ;
	return siz;
}

int build_both_empty(struct esp_package* res)
{
	if (res == NULL) return -1;
	res->type = PKG_BOTH_EMPTY;
	res->len = 0;
	res->left = 0;
	return 1;
}

int build_pc_good_or_bad(struct esp_package* res, const char gud)
{
	if (res == NULL) return -1; // null is bad
	res->type = PKG_PC_OPTION_FINAL;
	res->data.pc.option_final.good = gud;
	res->left = 0;
	res->len = sizeof(res->data.pc.option_final.good);
	return 1;
}

int build_pc_request_weight(struct esp_package* res)
{
	if (res == NULL) return -1; // null is bad
	res->type = PKG_PC_REQUEST_WEIGHT;
	res->left = 0;
	res->len = 0;
	// no data
	return 1;
}

int build_esp_has_something(struct esp_package* res)
{
	if (res == NULL) return -1;
	res->len = 0;
	res->left = 0;
	res->type = PKG_ESP_DETECTED_SOMETHING;
	return 1;
}

int build_esp_each_jpg_data(struct esp_package* res, char** data, unsigned long* remaining)
{
	if (res == NULL || data == NULL || *data == NULL || remaining == NULL) return 0; // invalid ptrs
	if (*remaining == 0) return 0; // no data to send

	int current_size = build_package_raw(res, *data, *remaining, PKG_ESP_JPG_SEND); // auto set type, left, len and data
	*data += current_size; // advance current_size
	*remaining -= current_size;

	return res->len;
}

int build_esp_weight_info(struct esp_package* res, const float bud, const float gud)
{
	if (res == NULL) return -1;
	res->left = 0;
	res->type = PKG_ESP_WEIGHT_INFO;
	res->len = sizeof(res->data.esp.weight_info);
	res->data.esp.weight_info.bad_side_kg = bud;
	res->data.esp.weight_info.good_side_kg = gud;
	return 1;
}
