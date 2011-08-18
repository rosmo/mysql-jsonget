#include <mysql.h>

my_bool json_get_init(UDF_INIT *initid, UDF_ARGS *args, char *message);

void json_get_deinit(UDF_INIT *initid);

char *json_get(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);


