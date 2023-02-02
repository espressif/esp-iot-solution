/*
 *    Copyright 2020 Piyush Shah <shahpiyushv@gmail.com>
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <json_parser.h>

#define json_test_str	"{\n\"str_val\" :    \"JSON Parser\",\n" \
			"\t\"float_val\" : 2.0,\n" \
			"\"int_val\" : 2017,\n" \
			"\"bool_val\" : false,\n" \
			"\"supported_el\" :\t [\"bool\",\"int\","\
			"\"float\",\"str\"" \
			",\"object\",\"array\"],\n" \
			"\"features\" : { \"objects\":true, "\
			"\"arrays\":\"yes\"},\n"\
			"\"int_64\":109174583252}"

int main(int argc, char **argv)
{
	jparse_ctx_t jctx;
	int ret = json_parse_start(&jctx, json_test_str, strlen(json_test_str));
	if (ret != OS_SUCCESS) {
		printf("Parser failed\n");
		return -1;
	}
	char str_val[64];
	int int_val, num_elem;
	int64_t int64_val;
	bool bool_val;
	float float_val;

	if (json_obj_get_string(&jctx, "str_val", str_val, sizeof(str_val)) == OS_SUCCESS)
		printf("str_val %s\n", str_val);

	if (json_obj_get_float(&jctx, "float_val", &float_val) == OS_SUCCESS)
		printf("float_val %f\n", float_val);

	if (json_obj_get_int(&jctx, "int_val", &int_val) == OS_SUCCESS)
		printf("int_val %d\n", int_val);

	if (json_obj_get_bool(&jctx, "bool_val", &bool_val) == OS_SUCCESS)
		printf("bool_val %s\n", bool_val ? "true" : "false");

	if (json_obj_get_array(&jctx, "supported_el", &num_elem) == OS_SUCCESS) {
		printf("Array has %d elements\n", num_elem);
		int i;
		for (i = 0; i < num_elem; i++) {
			json_arr_get_string(&jctx, i, str_val, sizeof(str_val));
			printf("index %d: %s\n", i, str_val);
		}
		json_obj_leave_array(&jctx);
	}
	if (json_obj_get_object(&jctx, "features") == OS_SUCCESS) {
		printf("Found object\n");
		if (json_obj_get_bool(&jctx, "objects", &bool_val) == OS_SUCCESS)
			printf("objects %s\n", bool_val ? "true" : "false");
		if (json_obj_get_string(&jctx, "arrays", str_val, sizeof(str_val)) == OS_SUCCESS)
			printf("arrays %s\n", str_val);
		json_obj_leave_object(&jctx);
	}
	if (json_obj_get_int64(&jctx, "int_64", &int64_val) == OS_SUCCESS)
		printf("int64_val %lld\n", int64_val);

	json_parse_end(&jctx);
	return 0;

}
