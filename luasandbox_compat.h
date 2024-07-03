// Macros to hide the differences between PHP versions
// Implementation details, include only from .c files
#ifndef LUASANDBOX_COMPAT_H
#define LUASANDBOX_COMPAT_H
#if PHP_VERSION_ID < 80000
    #define luasandbox_update_property(scope, object, name, name_length, value) \
        zend_update_property(scope, object, name, name_length, value)
    #define luasandbox_update_property_string(scope, object, name, name_length, value) \
        zend_update_property_string(scope, object, name, name_length, value)
    #define luasandbox_update_property_long(scope, object, name, name_length, value) \
        zend_update_property_long(scope, object, name, name_length, value)
    #define luasandbox_read_property(scope, object, name, name_length, silent, rv) \
        zend_read_property(scope, object, name, name_length, silent, rv)
	#define luasandbox_sortable const void *
	#define luasandbox_to_bucket(var) ((Bucket *) var)
#else
    #define luasandbox_update_property(scope, object, name, name_length, value) \
        zend_update_property(scope, Z_OBJ_P(object), name, name_length, value)
    #define luasandbox_update_property_string(scope, object, name, name_length, value) \
        zend_update_property_string(scope, Z_OBJ_P(object), name, name_length, value)
    #define luasandbox_update_property_long(scope, object, name, name_length, value) \
        zend_update_property_long(scope, Z_OBJ_P(object), name, name_length, value)
    #define luasandbox_read_property(scope, object, name, name_length, silent, rv) \
        zend_read_property(scope, Z_OBJ_P(object), name, name_length, silent, rv)
	#define luasandbox_sortable Bucket *
	#define luasandbox_to_bucket(var) (var)
#endif
#endif // LUASANDBOX_COMPAT_H
