#include "php.h"
#include "php_sqlite3.h"

#define COMPILE_DL_SQLITE3

static function_entry sqlite3_functions[] = {


    PHP_FE(sqlite3_libversion, NULL)
    PHP_FE(sqlite3_open,    NULL)
    PHP_FE(sqlite3_close,   NULL)    
    PHP_FE(sqlite3_error,   NULL)
    PHP_FE(sqlite3_exec,    NULL)
    PHP_FE(sqlite3_query,   NULL)
    PHP_FE(sqlite3_changes, NULL)
    
    PHP_FE(sqlite3_bind_int,    NULL)
    PHP_FE(sqlite3_bind_double, NULL)    
    PHP_FE(sqlite3_bind_text,   NULL)
    PHP_FE(sqlite3_bind_blob,   NULL)   
    PHP_FE(sqlite3_bind_null,   NULL)
    
    PHP_FE(sqlite3_query_exec,  NULL)

    PHP_FE(sqlite3_fetch,        NULL)    
    PHP_FE(sqlite3_fetch_array,  NULL)
    PHP_FE(sqlite3_column_count, NULL)
    PHP_FE(sqlite3_column_name,  NULL)        
    PHP_FE(sqlite3_column_type,  NULL)            
    PHP_FE(sqlite3_query_close,  NULL)
        
    PHP_FE(sqlite3_last_insert_rowid, NULL)
    PHP_FE(sqlite3_create_function,   NULL)    

    {NULL, NULL, NULL}
};

zend_module_entry sqlite3_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_SQLITE3_EXTNAME,
    sqlite3_functions,
    PHP_MINIT(sqlite3),
    NULL,
    NULL,
    NULL,
    PHP_MINFO(sqlite3),
#if ZEND_MODULE_API_NO >= 20010901
    PHP_SQLITE3_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};


/*
 * SQLite handler resource definition 
 */
 
static int   le_sqlite3_resource;
static char* le_sqlite3_resource_name="SQLITE3";

  
typedef struct 
{
  sqlite3* handle;
  char*    user_cb; /* user supplied function name for sqlite3_exec() */
} php_sqlite3_resource;


/*
 * SQLite statment handler resource definition 
 */
 
static int   le_sqlite3_stmt_resource;
static char* le_sqlite3_stmt_resource_name="SQLITE3-QUERY";

  
typedef struct 
{
  php_sqlite3_resource* sqlite3;
  sqlite3_stmt*         stmt;
  int                   rsrc_id;
} php_sqlite3_stmt_resource; 


/* 
 * a structure which hold a user defined SQL function
 *
 */
 
typedef struct
{
  char*                func_name;     /* SQL function name */
  char*                cb_name;       /* PHP Callback name */
  int                  cb_num_args;   /* # arguments */
  php_sqlite3_resource *me;
} php_sqlite3_func_t;

/*
 * resource destruction callbacks
 */
 
void 
php_sqlite3_resource_destruction ( zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
  php_sqlite3_resource *me = (php_sqlite3_resource*) rsrc->ptr;
  
  sqlite3_close (me->handle);
  
  if (me->user_cb) efree (me->user_cb);
  
  efree (me);
 
}

void 
php_sqlite3_stmt_resource_destruction ( zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
  php_sqlite3_stmt_resource *me = (php_sqlite3_stmt_resource*) rsrc->ptr;
  
   sqlite3_finalize(me->stmt);
  efree (me);  
}



/*  zval to php_sqlite3_resource conversion macro  */
#define ZVAL_TO_S3_HANDLE(me,zvalpp)  ZEND_FETCH_RESOURCE(me, php_sqlite3_resource*, zvalpp, -1, le_sqlite3_resource_name, le_sqlite3_resource)

/* zval to php_sqlite3_stmt_resource conversion macro */
#define ZVAL_TO_STMT(me,zvalpp)  ZEND_FETCH_RESOURCE(me, php_sqlite3_stmt_resource*, zvalpp, -1, le_sqlite3_stmt_resource_name, le_sqlite3_stmt_resource)



PHP_MINIT_FUNCTION(sqlite3)
{
  /* register resource destructors */
  le_sqlite3_resource =zend_register_list_destructors_ex(php_sqlite3_resource_destruction, NULL, le_sqlite3_resource_name, module_number);  
  le_sqlite3_stmt_resource =zend_register_list_destructors_ex(php_sqlite3_stmt_resource_destruction, NULL, le_sqlite3_stmt_resource_name, module_number);    
  
  /* register some constants... */
  REGISTER_LONG_CONSTANT("SQLITE3_INTEGER", SQLITE_INTEGER, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("SQLITE3_FLOAT"  , SQLITE_FLOAT,   CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("SQLITE3_TEXT",    SQLITE_TEXT,    CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("SQLITE3_BLOB",    SQLITE_BLOB,    CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("SQLITE3_NULL",    SQLITE_NULL,    CONST_CS | CONST_PERSISTENT);
  
}


#ifdef COMPILE_DL_SQLITE3
ZEND_GET_MODULE(sqlite3)
#endif


/* {{{ PHP_MINFO_FUNCTION */

PHP_MINFO_FUNCTION(sqlite3)
{

  php_info_print_table_start();
  php_info_print_table_row(2, "SQLite3 support", "enabled");
  php_info_print_table_row(2, "sqlite3 library version", sqlite3_libversion());
  php_info_print_table_end();
}

/* }}} 
/* {{{ string sqlite3_libersion()
   Returns the SQLite3 library version (such as "3.2.27").  */
   
PHP_FUNCTION(sqlite3_libversion)
{
  RETURN_STRING(estrdup(sqlite3_libversion()), 0);
}

/* }}}
   {{{ bool sqlite3_close(resource handle)
   Close (and free) a Sqlite3 resource handle */

PHP_FUNCTION(sqlite3_close)
{
  php_sqlite3_resource* me;
  zval* z_rs;
  int err;
  
 if(ZEND_NUM_ARGS() != 1) WRONG_PARAM_COUNT;


  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"r", &z_rs)==FAILURE)
    return;
    
  ZVAL_TO_S3_HANDLE (me, &z_rs);  

  switch (sqlite3_close (me->handle))
  {
    case SQLITE_OK:
      RETURN_TRUE;
      break;
      
    case SQLITE_BUSY:
      zend_error(E_WARNING, "Closing a SQLite database with active query(ies) is not safe.");
      break;
      
    default:
      RETURN_FALSE;      
  }
}

/* }}}
   {{{ resource sqlite3_open(string path_to_database)
   Open (or create) a SQLite3 database */
  
PHP_FUNCTION(sqlite3_open)
{
  int r_id ; /* resource ID */
  php_sqlite3_resource* me;
  zval* res;
  int err;
  char* dbpath;
  int dbpath_len;
  
  if(ZEND_NUM_ARGS() != 1) WRONG_PARAM_COUNT;
  
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"s", &dbpath, &dbpath_len) == FAILURE) {
    return;
  }
  
  MAKE_STD_ZVAL(res);
  
  me = (php_sqlite3_resource*)  emalloc ( sizeof (php_sqlite3_resource));
  memset(me, 0, sizeof (php_sqlite3_resource));    
  err = sqlite3_open(dbpath, &(me->handle));
  
  if (err)
  {
    zend_error (E_ERROR, "Could not open database %s: %s", dbpath, sqlite3_errmsg (me->handle));
    sqlite3_close(me->handle);
    RETURN_NULL();
  }

  r_id = ZEND_REGISTER_RESOURCE(res, me, le_sqlite3_resource);  
  RETURN_RESOURCE(r_id);
}

/* }}}
   {{{ string sqlite3_error(resource handle)
   Returns the last error message */

PHP_FUNCTION(sqlite3_error)
{
  php_sqlite3_resource* me;
  zval* z_rs;
  
 if(ZEND_NUM_ARGS() != 1) WRONG_PARAM_COUNT;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"r", &z_rs)==FAILURE)
    return;
  
  ZVAL_TO_S3_HANDLE (me, &z_rs);    
  
  RETURN_STRING (estrdup(sqlite3_errmsg(me->handle)), 0);
}
/* }}} */


/*
 * callback function called by sqlite3_exec
 */

static int
php_sqlite3_callback(void* _me,int ncols ,char** col_vals, char** col_names)
{
  php_sqlite3_resource* me = (php_sqlite3_resource*) _me;
  int n;
  zval**  func_params[2];
  zval*   z_col_vals;
  zval*   z_col_names;
  zval*  func_retval;
  zval*  func_name;
  int    retval;
  
  
  MAKE_STD_ZVAL(func_name);
  
  func_params[0]=&z_col_vals;
  func_params[1]=&z_col_names;
  
  ZVAL_STRING (func_name, me->user_cb, 0);

  MAKE_STD_ZVAL(z_col_vals) ; 
  MAKE_STD_ZVAL(z_col_names);  
  array_init (z_col_vals);
  array_init (z_col_names);
  
  for (n=0; n < ncols; n++)
  {
    /* append column value */    
    add_next_index_string (z_col_names, col_vals[n],  1);
    
    /* append column name */
    add_next_index_string (z_col_vals, col_names[n], 1);        
  }

  /* do we need this ?? - bruno */  
   /*  TSRMSLS_FETCH(); */
  
  if (call_user_function_ex(CG(function_table), NULL, func_name, &func_retval, 2, func_params, 0,  NULL TSRMLS_CC ) != SUCCESS)
  {
      zend_error(E_ERROR, "Function call to '%s' failed", me->user_cb);
  }
  
  /*
   * callback function MUST return an integer.
   *
   * (if this return value is non-zero, pass it back to sqlite3_exec(), to abort the query).
   * 
   */
  
  if (func_retval->type != IS_LONG)
    return -1;
  
  retval = Z_LVAL_P(func_retval);
  zval_dtor(func_retval);
  
  return retval;
}


/* {{{ bool sqlite3_exec(resource handle, string sql [,mixed callback])
   Execute a sql query.  Optionnaly call the callback function for each row retrieved in the result set */

PHP_FUNCTION(sqlite3_exec)
{
  php_sqlite3_resource* me;
  zval* z_rs;
  char* sql;
  int   sql_len;
  
  char*  user_cb;
  int    user_cb_len;
  int   err;

  if (ZEND_NUM_ARGS() < 2) WRONG_PARAM_COUNT;
  
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"rs|s", &z_rs, &sql, &sql_len, &user_cb, &user_cb_len)==FAILURE)
    return;
    
  ZVAL_TO_S3_HANDLE (me, &z_rs);  
  switch (ZEND_NUM_ARGS())
  {
  
    case 2:
      
      err = sqlite3_exec (me->handle, sql, NULL, NULL, NULL);      
      if (err) 
        RETURN_FALSE;
        
      RETURN_TRUE;      
      break;
      
     case 3:
      me->user_cb = estrdup (user_cb);
      err = sqlite3_exec (me->handle, sql, php_sqlite3_callback, me, NULL);
      
      if (err)
        RETURN_FALSE;
        
      RETURN_TRUE;
       
      
      break;
      
   default:
     WRONG_PARAM_COUNT;          
  }
}

/* }}}
   {{{ bool sqlite3_bind_int(resource resultset, int index, int value)
    bind an integer value at the index parameter 'index' (starts at 1)
*/

PHP_FUNCTION(sqlite3_bind_int)
{
  php_sqlite3_stmt_resource* stmt;
  zval*                      z_stmt;
  long    idx;
  long    val;

  if(ZEND_NUM_ARGS() != 3) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"rll",&z_stmt, &idx, &val)==FAILURE)
    return;    

  ZVAL_TO_STMT (stmt, &z_stmt);

  if (sqlite3_bind_int(stmt->stmt, idx, val) != SQLITE_OK)
    RETURN_FALSE;
    
  RETURN_TRUE; 
}

/* }}}
   {{{ bool sqlite3_bind_double(resource resultset, int index, double value)
    bind an double value at the index parameter 'index' (starts at 1)
*/

PHP_FUNCTION(sqlite3_bind_double)
{
  php_sqlite3_stmt_resource* stmt;
  zval*                      z_stmt;
  long    idx;
  double  val;

  if(ZEND_NUM_ARGS() != 3) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"rld",&z_stmt, &idx, &val)==FAILURE)
    return;    

  ZVAL_TO_STMT (stmt, &z_stmt);

  if (sqlite3_bind_double(stmt->stmt, idx, val) != SQLITE_OK)
    RETURN_FALSE;
    
  RETURN_TRUE; 
}


/* }}}
   {{{ bool sqlite3_bind_null(resource resultset, int index)
    bind a NULL value at the index parameter 'index' (starts at 1)
*/

PHP_FUNCTION(sqlite3_bind_null)
{
  php_sqlite3_stmt_resource* stmt;
  zval*                      z_stmt;
  long    idx;

  if(ZEND_NUM_ARGS() != 2) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"rl",&z_stmt, &idx)==FAILURE)
    return;    

  ZVAL_TO_STMT (stmt, &z_stmt);

  if (sqlite3_bind_null(stmt->stmt, idx) != SQLITE_OK)
    RETURN_FALSE;
    
  RETURN_TRUE; 
}

/* }}}
   {{{ bool sqlite3_bind_text(resource resultset, int index, string text)
    bind a string value at the index parameter 'index' (starts at 1)
*/

PHP_FUNCTION(sqlite3_bind_text)
{
  php_sqlite3_stmt_resource* stmt;
  zval*                      z_stmt;
  long    idx;
  zval*   z_val;


  if(ZEND_NUM_ARGS() != 3) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"rlz",&z_stmt, &idx, &z_val)==FAILURE)
    return;    

  ZVAL_TO_STMT (stmt, &z_stmt);

  /* increment reference count on this object, as we do not want sqlite3 to 
   * duplicate it (SQLITE_STATIC flag).
   */
     
  z_val->refcount++;
  
  if (sqlite3_bind_text(stmt->stmt, idx, Z_STRVAL_P(z_val), Z_STRLEN_P(z_val), SQLITE_STATIC) != SQLITE_OK)
    RETURN_FALSE;
    
  RETURN_TRUE; 
}   


/* }}}
   {{{ bool sqlite3_bind_blob(resource resultset, int index, string data)
    bind a blob value at the index parameter 'index' (starts at 1)
*/

PHP_FUNCTION(sqlite3_bind_blob)
{
  php_sqlite3_stmt_resource* stmt;
  zval*                      z_stmt;
  long    idx;
  zval*   z_val;


  if(ZEND_NUM_ARGS() != 3) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"rlz",&z_stmt, &idx, &z_val)==FAILURE)
    return;    


  ZVAL_TO_STMT (stmt, &z_stmt);

  /* increment reference count on this object, as we do not want sqlite3 to 
   * duplicate it (SQLITE_STATIC flag).
   */
   
  z_val->refcount++;
  
  if (sqlite3_bind_blob(stmt->stmt, idx, Z_STRVAL_P(z_val), Z_STRLEN_P(z_val), SQLITE_STATIC) != SQLITE_OK)
    RETURN_FALSE;
    
  RETURN_TRUE; 
}   


/* }}}
   {{{ resource sqlite3_query(resource handle, string sql)
   execute a sql query. Returns a result set resource */

PHP_FUNCTION(sqlite3_query)
{
  php_sqlite3_resource* me;
  php_sqlite3_stmt_resource* stmt;
  zval* z_rs;
  char* sql;
  int   sql_len;
  int   err;
  zval* z_stmt;
  const char* pztail;


  if(ZEND_NUM_ARGS() != 2) WRONG_PARAM_COUNT;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"rs", &z_rs, &sql, &sql_len)==FAILURE)
    return;
    
  ZVAL_TO_S3_HANDLE (me, &z_rs);  
  
  stmt = (php_sqlite3_stmt_resource*)  emalloc ( sizeof (php_sqlite3_stmt_resource));
  memset(stmt, 0, sizeof (php_sqlite3_stmt_resource));    
  stmt->sqlite3 = me;
  
  err = sqlite3_prepare (me->handle, sql, sql_len, &(stmt->stmt), &pztail);
  if (err) RETURN_FALSE;
  
  
  MAKE_STD_ZVAL(z_stmt);
   stmt->rsrc_id =  ZEND_REGISTER_RESOURCE(z_stmt, stmt, le_sqlite3_stmt_resource);  
  RETURN_RESOURCE(stmt->rsrc_id);
}

/* }}} */
/* {{{ bool sqlite3_query_exec(resource statement, [bool free=TRUE]) 
    execute a SQL statement prepared by sqlite3_query(). 
    If free is TRUE, free the result set
    Return TRUE if all went ok.
 */

PHP_FUNCTION(sqlite3_query_exec)
{
  php_sqlite3_stmt_resource* stmt;
  zval*                      z_stmt;
  int                        err;
  zend_bool                  dofree;

  if(ZEND_NUM_ARGS() <1) WRONG_PARAM_COUNT;
  if(ZEND_NUM_ARGS() == 1) dofree=1;
  
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"r|b", &z_stmt, &dofree)==FAILURE)
    return; 
    
  ZVAL_TO_STMT (stmt, &z_stmt);
  
  err = sqlite3_step(stmt->stmt);
  if (dofree)
    zend_list_delete(stmt->rsrc_id);
  
  if ((err==SQLITE_DONE) || (err==SQLITE_ROW))
    RETURN_TRUE;
    
  RETURN_FALSE;    
}
/* }}} */


static void
php_sqlite_fetch_result(php_sqlite3_stmt_resource* stmt, zval* return_value, int mode)
{
  int                        col;
  zval*                      col_val;
  
  array_init(return_value);
  
  for (col=0; col < sqlite3_column_count(stmt->stmt); col++)
  {
    MAKE_STD_ZVAL(col_val); 
    
    switch (sqlite3_column_type(stmt->stmt, col))
    {
    
      case SQLITE_INTEGER:
        ZVAL_LONG(col_val, sqlite3_column_int (stmt->stmt, col));
        break;
        
      case SQLITE_FLOAT:
        ZVAL_DOUBLE(col_val, sqlite3_column_double(stmt->stmt, col));
        break;
        
      case SQLITE_TEXT:
        ZVAL_STRING(col_val, estrdup(sqlite3_column_text(stmt->stmt, col)), 0);
        break;
      
      case SQLITE_BLOB:
        ZVAL_STRINGL(col_val, estrdup(sqlite3_column_blob(stmt->stmt, col)), 
          sqlite3_column_bytes(stmt->stmt, col), 1);
        break;
      
      case SQLITE_NULL:
        ZVAL_NULL(col_val);
        break;
  
      default:
        zend_error(E_WARNING, "Unexpected column type %i", sqlite3_column_type(stmt->stmt, col));
    }    
    
    switch (mode)
    {
      case PHP_SQLITE3_FETCH_ASSOC:      
        add_assoc_zval (return_value, estrdup(sqlite3_column_name(stmt->stmt, col)), col_val);  
        break;
        
      case PHP_SQLITE3_FETCH_INDEX:
        add_next_index_zval (return_value, col_val);
        break;
    }
  }  
}

/* {{{ array sqlite3_fetch(resource resultset)
   move the result set cursor to the next item and returns an array of the retrived row .*/

PHP_FUNCTION(sqlite3_fetch)
{

  php_sqlite3_stmt_resource* stmt;
  zval*                      z_stmt;
  int                        err;

  if(ZEND_NUM_ARGS() != 1) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"r", &z_stmt)==FAILURE)
    return; 
    
  ZVAL_TO_STMT (stmt, &z_stmt);

  err = sqlite3_step(stmt->stmt);
  if (err != SQLITE_ROW) 
    RETURN_FALSE;  
    
   php_sqlite_fetch_result(stmt, return_value,PHP_SQLITE3_FETCH_INDEX);   
     
   return;
}

/* }}}
   {{{ array sqlite3_fetch(resource resultset)
   move the result set cursor to the next item and returns an associative array of the retrived row. */

PHP_FUNCTION(sqlite3_fetch_array)
{

  php_sqlite3_stmt_resource* stmt;
  zval*                      z_stmt;
  int                        err;

  if(ZEND_NUM_ARGS() != 1) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"r", &z_stmt)==FAILURE)
    return;    

  ZVAL_TO_STMT (stmt, &z_stmt);    

  err = sqlite3_step(stmt->stmt);
  if (err != SQLITE_ROW) 
    RETURN_FALSE;
          
  php_sqlite_fetch_result(stmt, return_value,PHP_SQLITE3_FETCH_ASSOC);   
     
  return;
}

/* }}}
   {{{ integer sqlite3_column_count(resource resultset)   
   Returns the number of column in the specified result set. */
   
PHP_FUNCTION(sqlite3_column_count)
{
  php_sqlite3_stmt_resource* stmt;
  zval*                      z_stmt;

  if(ZEND_NUM_ARGS() != 1) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"r", &z_stmt)==FAILURE)
    return;    

  ZVAL_TO_STMT (stmt, &z_stmt);

  RETURN_LONG (sqlite3_column_count (stmt->stmt));
}

/* }}}
   {{{ array sqlite3_column_name(resource resultset, integer colum}
   return the name of the column #column */

PHP_FUNCTION(sqlite3_column_name)
{
  php_sqlite3_stmt_resource* stmt;
  zval*                      z_stmt;
  int                        col;

  if(ZEND_NUM_ARGS() != 2) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"rl", &z_stmt, &col)==FAILURE)
    return;    
    
  ZVAL_TO_STMT (stmt, &z_stmt);    
  
  RETURN_STRING (estrdup(sqlite3_column_name(stmt->stmt, col)), 0);
}

/* }}}
   {{{ constant sqlite3_column_type(resource resultset, integer colum}
   return the type of the column #column */

PHP_FUNCTION(sqlite3_column_type)
{
  php_sqlite3_stmt_resource* stmt;
  zval*                      z_stmt;
  int                        col;

  if(ZEND_NUM_ARGS() != 2) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"rl", &z_stmt, &col)==FAILURE)
    return;    
    
  ZVAL_TO_STMT (stmt, &z_stmt);    
  
  RETURN_LONG (sqlite3_column_type(stmt->stmt, col));
}

/* }}}
   {{{ bool sqlite3_querry_close(resource resultset)
   close (and free) a result set handle */
   
PHP_FUNCTION(sqlite3_query_close)
{
  php_sqlite3_stmt_resource* stmt;
  zval*                      z_stmt;
  int                        err;


  if(ZEND_NUM_ARGS() != 1) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"r", &z_stmt)==FAILURE)
    return;    

  ZVAL_TO_STMT (stmt, &z_stmt);

  err = sqlite3_finalize ( stmt->stmt);  

	zend_list_delete(Z_RESVAL_P(z_stmt));

  if (err) RETURN_FALSE ;  
  RETURN_TRUE;
}

/* }}}
   {{{ integer sqlite3_last_insert_rowid(resource sqlite3)
   return the last inserted row id */

PHP_FUNCTION(sqlite3_last_insert_rowid)
{
  php_sqlite3_resource* me;
  zval* z_rs;
  
 if(ZEND_NUM_ARGS() != 1) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"r", &z_rs)==FAILURE)
    return;

  ZVAL_TO_S3_HANDLE (me, &z_rs);  
 
  RETURN_LONG(sqlite3_last_insert_rowid (me->handle));
}

/* }}} 
   {{{ integer sqlite3_changes(resource sqlite3)
   return  the number of database rows that were changed (or inserted or deleted) by the most recently completed INSERT, UPDATE, or DELETE statement.
*/

PHP_FUNCTION(sqlite3_changes)
{
  php_sqlite3_resource* me;
  zval* z_rs;
  
 if(ZEND_NUM_ARGS() != 1) WRONG_PARAM_COUNT;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"r", &z_rs)==FAILURE)
    return;

  ZVAL_TO_S3_HANDLE (me, &z_rs);  
 
 RETURN_LONG(sqlite3_changes (me->handle));
}
/* }}} */


/* {{{ void void php_sqlite3_user_function_cb(sqlite3_context*,int nargs,sqlite3_value**)
 * callback function, for user-defined SQL functions. Get called by sqlite3 library
 */

void php_sqlite3_user_function_cb(sqlite3_context* ctxt,int nargs,sqlite3_value** vals) 
{
  int err; 
  zval***  z_cb_params;
  zval**   z_param;
  zval*    z_retval;
  zval*    z_cb_name;
  int  n;
  php_sqlite3_func_t* func;
  
  
  func = (php_sqlite3_func_t*) sqlite3_user_data(ctxt);
  
  MAKE_STD_ZVAL(z_cb_name);
  ZVAL_STRING  (z_cb_name, func->cb_name, 0);
  
  
  /*
   * fill in callback parameters
   */

  z_cb_params = (zval***) emalloc (sizeof(zval**) * nargs);

  for (n=0; n < nargs; n++)
  {
    z_param = (zval**) emalloc( sizeof(zval*));
    MAKE_STD_ZVAL(*z_param);
            
    switch (sqlite3_value_type (vals[n]))
    {
       case  SQLITE_INTEGER:
        ZVAL_LONG(*z_param,   sqlite3_value_int(vals[n]));
        break;
      
      case SQLITE_FLOAT:
        ZVAL_DOUBLE(*z_param, sqlite3_value_double(vals[n]));
        break;
      
      case SQLITE_TEXT:
        ZVAL_STRING(*z_param, (char*) sqlite3_value_text(vals[n]), 1);
				break;
          
      case SQLITE_NULL:
      default:      
        ZVAL_NULL(*z_param);
    }
    
    z_cb_params[n] = z_param;    
  }
  
  /*
   * now call the user function ...
   */
  
  err = call_user_function_ex (CG(function_table), NULL, z_cb_name, &z_retval, nargs, z_cb_params, 0,  NULL TSRMLS_CC );
  
  if (err != SUCCESS)
  {
    if (z_retval) zval_dtor(z_retval);
    sqlite3_result_error(ctxt, "function called failed.", -1);
    return;
  }
    
  /*
   * ... and parse the output
   */

  switch (z_retval->type)
  {
    case IS_NULL:
      sqlite3_result_null(ctxt);
      break;
      
    case IS_LONG:
      sqlite3_result_int(ctxt, Z_LVAL_P(z_retval));
      break;
    
   case IS_DOUBLE:
       sqlite3_result_double(ctxt, Z_DVAL_P(z_retval));
      break;
      
   case IS_STRING:
       sqlite3_result_text(ctxt, Z_STRVAL_P(z_retval), Z_STRLEN_P(z_retval),SQLITE_TRANSIENT);
      break;

  default:
      sqlite3_result_error(ctxt, "invalid return type.", -1);
      break;
  }
  
   zval_dtor(z_retval);
}
 
/* }}}
   {{{ bool sqlite3_create_function(resource sqlite3, string function_name, int nb_args)
*/

PHP_FUNCTION(sqlite3_create_function)
{
  php_sqlite3_resource* me;
  php_sqlite3_func_t*   func;  
  
  zval*  z_rs;
  char*  func_name;
  int    func_name_len;
  char*  cb_name;
  int    cb_name_len;
  int    cb_num_args;
  int    err;
  
 if(ZEND_NUM_ARGS() != 4) WRONG_PARAM_COUNT;
 
 if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"rsls", &z_rs, 
    &func_name, &func_name_len, 
    &cb_num_args,
    &cb_name, &cb_name_len)==FAILURE)
    return;

  ZVAL_TO_S3_HANDLE (me, &z_rs);
  
  func = (php_sqlite3_func_t*) emalloc (sizeof(php_sqlite3_func_t));
  
  func->func_name   = estrdup(func_name);
  func->cb_name     = estrdup(cb_name);  
  func->cb_num_args = cb_num_args;
  func->me          = me;

  err = sqlite3_create_function(me->handle, func_name, cb_num_args, SQLITE_ANY, func, php_sqlite3_user_function_cb, NULL, NULL);
  
  if (err == SQLITE_ERROR)
    RETURN_FALSE;
    
  RETURN_TRUE;
}
/* }}} */   
