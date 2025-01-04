#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <mysql.h>
#include <string.h>
#include <stdlib.h>

// Helper function: Format SQL dynamically
static char *format_sql(const char *template, const char *arg1, const char *arg2) {
    size_t len = strlen(template) + strlen(arg1) + (arg2 ? strlen(arg2) : 0) + 1;
    char *result = malloc(len);
    snprintf(result, len, template, arg1, arg2 ? arg2 : "");
    return result;
}

// Connect to the MySQL database
MYSQL* connect_to_db(const char *host, const char *user, const char *password, const char *database) {
    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL) {
        PyErr_SetString(PyExc_Exception, "mysql_init() failed");
        return NULL;
    }

    if (mysql_real_connect(conn, host, user, password, database, 0, NULL, 0) == NULL) {
        PyErr_SetString(PyExc_Exception, mysql_error(conn));
        mysql_close(conn);
        return NULL;
    }

    return conn;
}

// Create table based on model
static PyObject *create_table(PyObject *self, PyObject *args) {
    const char *host, *user, *password, *database;
    PyObject *model;
    
    if (!PyArg_ParseTuple(args, "ssssO!", &host, &user, &password, &database, &PyDict_Type, &model)) {
        return NULL;
    }

    // Build the CREATE TABLE SQL
    PyObject *keys = PyDict_Keys(model);
    PyObject *values = PyDict_Values(model);
    Py_ssize_t size = PyList_Size(keys);

    char *create_sql = malloc(512);
    snprintf(create_sql, 512, "CREATE TABLE IF NOT EXISTS %s (", PyUnicode_AsUTF8(PyList_GetItem(keys, 0)));
    for (Py_ssize_t i = 1; i < size; i++) {
        char *col_def = format_sql("%s %s", PyUnicode_AsUTF8(PyList_GetItem(keys, i)),
                                   PyUnicode_AsUTF8(PyList_GetItem(values, i)));
        strcat(create_sql, col_def);
        strcat(create_sql, (i < size - 1) ? ", " : ");");
        free(col_def);
    }

    MYSQL *conn = connect_to_db(host, user, password, database);
    if (conn == NULL) {
        free(create_sql);
        return NULL;
    }

    if (mysql_query(conn, create_sql)) {
        PyErr_SetString(PyExc_Exception, mysql_error(conn));
        mysql_close(conn);
        free(create_sql);
        return NULL;
    }

    mysql_close(conn);
    free(create_sql);

    Py_RETURN_NONE;
}

// Insert a record
static PyObject *insert(PyObject *self, PyObject *args) {
    const char *host, *user, *password, *database, *table_name;
    PyObject *record;

    if (!PyArg_ParseTuple(args, "ssssO!", &host, &user, &password, &database, &table_name, &PyDict_Type, &record)) {
        return NULL;
    }

    PyObject *keys = PyDict_Keys(record);
    PyObject *values = PyDict_Values(record);
    Py_ssize_t size = PyList_Size(keys);

    // Build the INSERT SQL
    char *insert_sql = malloc(512);
    snprintf(insert_sql, 512, "INSERT INTO %s (", table_name);
    for (Py_ssize_t i = 0; i < size; i++) {
        strcat(insert_sql, PyUnicode_AsUTF8(PyList_GetItem(keys, i)));
        strcat(insert_sql, (i < size - 1) ? ", " : ") VALUES (");
    }
    for (Py_ssize_t i = 0; i < size; i++) {
        strcat(insert_sql, "?");
        strcat(insert_sql, (i < size - 1) ? ", " : ");");
    }

    MYSQL *conn = connect_to_db(host, user, password, database);
    if (conn == NULL) {
        free(insert_sql);
        return NULL;
    }

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        PyErr_SetString(PyExc_Exception, "mysql_stmt_init() failed");
        mysql_close(conn);
        free(insert_sql);
        return NULL;
    }

    if (mysql_stmt_prepare(stmt, insert_sql, strlen(insert_sql)) != 0) {
        PyErr_SetString(PyExc_Exception, mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        mysql_close(conn);
        free(insert_sql);
        return NULL;
    }

    MYSQL_BIND *bind = malloc(size * sizeof(MYSQL_BIND));
    for (Py_ssize_t i = 0; i < size; i++) {
        bind[i].buffer_type = MYSQL_TYPE_STRING;
        bind[i].buffer = (char *)PyUnicode_AsUTF8(PyList_GetItem(values, i));
        bind[i].buffer_length = strlen((char *)PyUnicode_AsUTF8(PyList_GetItem(values, i)));
        bind[i].is_null = 0;
        bind[i].length = NULL;
    }

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        PyErr_SetString(PyExc_Exception, mysql_stmt_error(stmt));
        free(bind);
        mysql_stmt_close(stmt);
        mysql_close(conn);
        free(insert_sql);
        return NULL;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        PyErr_SetString(PyExc_Exception, mysql_stmt_error(stmt));
        free(bind);
        mysql_stmt_close(stmt);
        mysql_close(conn);
        free(insert_sql);
        return NULL;
    }

    mysql_stmt_close(stmt);
    mysql_close(conn);
    free(bind);
    free(insert_sql);

    Py_RETURN_NONE;
}

// Read all records
static PyObject *read_all(PyObject *self, PyObject *args) {
    const char *host, *user, *password, *database, *table_name;
    if (!PyArg_ParseTuple(args, "ssss", &host, &user, &password, &database, &table_name)) {
        return NULL;
    }

    // Build the SELECT query
    char *select_sql = format_sql("SELECT * FROM %s;", table_name, NULL);

    MYSQL *conn = connect_to_db(host, user, password, database);
    if (conn == NULL) {
        free(select_sql);
        return NULL;
    }

    MYSQL_RES *res;
    MYSQL_ROW row;
    if (mysql_query(conn, select_sql)) {
        PyErr_SetString(PyExc_Exception, mysql_error(conn));
        mysql_close(conn);
        free(select_sql);
        return NULL;
    }

    res = mysql_store_result(conn);
    if (res == NULL) {
        PyErr_SetString(PyExc_Exception, mysql_error(conn));
        mysql_close(conn);
        free(select_sql);
        return NULL;
    }

    PyObject *result_list = PyList_New(0);
    int num_fields = mysql_num_fields(res);

    while ((row = mysql_fetch_row(res))) {
        PyObject *row_tuple = PyTuple_New(num_fields);
        for (int i = 0; i < num_fields; i++) {
            PyTuple_SetItem(row_tuple, i, PyUnicode_FromString(row[i] ? row[i] : "NULL"));
        }
        PyList_Append(result_list, row_tuple);
        Py_DECREF(row_tuple);
    }

    mysql_free_result(res);
    mysql_close(conn);
    free(select_sql);

    return result_list;
}

// Other CRUD functions (update, delete) can follow the same model.

static PyMethodDef CQueryMethods[] = {
    {"create_table", create_table, METH_VARARGS, "Create a table based on the model"},
    {"insert", insert, METH_VARARGS, "Insert a record based on the model"},
    {"read_all", read_all, METH_VARARGS, "Fetch all records from the table"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef cquerymodule = {
    PyModuleDef_HEAD_INIT,
    "cquery",
    "A CPython extension for model-based CRUD operations with MySQL",
    -1,
    CQueryMethods
};

PyMODINIT_FUNC PyInit_cquery(void) {
    return PyModule_Create(&cquerymodule);
}
