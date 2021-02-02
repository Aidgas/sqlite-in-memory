/**
    sudo apt-get install libicu-dev

    CREATE VIRTUAL TABLE IF NOT EXISTS `search_e` USING FTS5(id, s);
    INSERT INTO `search_e` SELECT id, s FROM search;
*/

#include <unicode/unistr.h>
#include <unicode/ustream.h>
#include <unicode/locid.h>
#include <iostream>
#include <iterator>

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <sys/stat.h>
#include <string>
#include <sstream>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>      /* for TCP_xxx defines */
#include <sys/socket.h>       /* for select */
#include <arpa/inet.h>        /* inet(3) functions */
#include <fcntl.h>
//#include <ev.h>
#include <unistd.h>
#include <memory>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <fstream>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <map>
#include <math.h>

#include <pthread.h>
#include <unicode/unistr.h>
#include <sys/time.h>
#include <time.h>

#include "sys/types.h"
#include "sys/sysinfo.h"

#include "include/inih/cpp/INIReader.h"

using namespace std;

//const int THREAD1_PORT    = 8787;

extern "C"
{
    //#include "include/sqlite3ext.h"
    #include "include/sqlite3.h"
    //#include "include/fts4-rank.c"
    //#include "include/fts5.h"
    //#include "include/csv.c"
    //#include "include/ext/fts3/fts3.h"
    //#include "include/ext/fts5/fts5.h"

    #include "include/http_parser.h"
    #include "include/buf.h"
    #include "include/jsmn.h"
}

char abs_exe_path[512]              = "";
static sqlite3 *s_db_handle         = NULL;
static sqlite3 *s_db_handle_memory  = NULL;
static const char *s_db_memory      = ":memory:";
//static const char *s_db_path        = "search.db";
string full_path_base               = "";
bool backup_base_now                = false;

bool not_limit_listener = true;
vector<string> listener_list_ips;

struct HttpData
{
  string path, body, url;
};
///-------------------------------------------------------------------------------------------
// Prints to the provided buffer a nice number of bytes (KB, MB, GB, etc)
char* pretty_bytes(char* buf, int buff_size, double bytes)
{
    int i = 0;
    const char* units[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    while (bytes > 1024) {
        bytes /= 1024;
        i++;
    }
    snprintf(buf, buff_size, "%.*f %s", i, bytes, units[i]);
    return buf;
}
///-------------------------------------------------------------------------------------------
std::string escapeJsonString(const std::string& input)
{
    std::string ss;
    //for (auto iter = input.cbegin(); iter != input.cend(); iter++) {
    //C++98/03:
    for (std::string::const_iterator iter = input.begin(); iter != input.end(); iter++)
    {
        switch (*iter)
        {
            case '\\': ss += "\\\\"; break;
            case '"':  ss += "\\\""; break;
            case '/':  ss += "\\/"; break;
            case '\b': ss += "\\b"; break;
            case '\f': ss += "\\f"; break;
            case '\n': ss += "\\n"; break;
            case '\r': ss += "\\r"; break;
            case '\t': ss += "\\t"; break;
            default:   ss += *iter; break;
        }
    }
    return ss;
}
///-------------------------------------------------------------------------------------------
/*string urlDecode(string &SRC) {
    string ret;
    char ch;
    int i, ii;
    for (i=0; i<SRC.length(); i++) {
        if (int(SRC[i])==37) {
            sscanf(SRC.substr(i+1,2).c_str(), "%x", &ii);
            ch=static_cast<char>(ii);
            ret+=ch;
            i=i+2;
        } else {
            ret+=SRC[i];
        }
    }
    return (ret);
}*/

string urlDecode(string str){
    string ret;
    char ch;
    int i, ii, len = str.length();

    for (i=0; i < len; i++){
        if(str[i] != '%'){
            if(str[i] == '+')
                ret += ' ';
            else
                ret += str[i];
        }else{
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            ret += ch;
            i = i + 2;
        }
    }
    return ret;
}
///-------------------------------------------------------------------------------------------
string join(const vector<string>& vec, string delim)
{
    string result;

    for(unsigned int i = 0; i < vec.size(); i++)
    {
        if( i > 0 )
        {
            result.append(delim);
        }

        result.append(vec[i]);
    }

    return result;
}
/// SQLITE CALLBACK
// ----------------------------------------------------------------------------------------------
static int callback_select_data(void *data, int count_res, char **argv, char **azColName)
{
    struct buf* output_byffer = (struct buf*)(data);
    int i;
    string tmp = "";

    buf_append_u8( output_byffer, '{' );

    for( i = 0; i < count_res; i++ )
    {
        buf_append_u8( output_byffer, '"' );
        buf_append_data( output_byffer, (void*)azColName[i], strlen( azColName[i] ) );
        buf_append_u8( output_byffer, '"' );
        buf_append_u8( output_byffer, ':' );
        buf_append_u8( output_byffer, '"' );

        tmp = escapeJsonString( argv[i] ? argv[i] : "NULL" );

        buf_append_data( output_byffer, (void*)tmp.c_str(), tmp.length() );

        buf_append_u8( output_byffer, '"' );

        if( i != count_res - 1 )
        {
            buf_append_u8( output_byffer, ',' );
        }
    }

    buf_append_u8( output_byffer, '}' );
    buf_append_u8( output_byffer, ',' );
    return 0;
}

// ----------------------------------------------------------------------------------------------
static int callback_select_data_array(void *data, int count_res, char **argv, char **azColName)
{
    struct buf* output_byffer = (struct buf*)(data);
    int i;
    string tmp = "";

    buf_append_u8( output_byffer, '[' );

    for( i = 0; i < count_res; i++ )
    {
        buf_append_u8( output_byffer, '"' );

        tmp = escapeJsonString( argv[i] ? argv[i] : "NULL" );

        buf_append_data( output_byffer, (void*)tmp.c_str(), tmp.length() );

        buf_append_u8( output_byffer, '"' );

        if( i != count_res - 1 )
        {
            buf_append_u8( output_byffer, ',' );
        }
    }

    buf_append_u8( output_byffer, ']' );
    buf_append_u8( output_byffer, ',' );
    return 0;
}

/*
** This function is used to load the contents of a database file on disk
** into the "main" database of open database connection pInMemory, or
** to save the current contents of the database opened by pInMemory into
** a database file on disk. pInMemory is probably an in-memory database,
** but this function will also work fine if it is not.
**
** Parameter zFilename points to a nul-terminated string containing the
** name of the database file on disk to load from or save to. If parameter
** isSave is non-zero, then the contents of the file zFilename are
** overwritten with the contents of the database opened by pInMemory. If
** parameter isSave is zero, then the contents of the database opened by
** pInMemory are replaced by data loaded from the file zFilename.
**
** If the operation is successful, SQLITE_OK is returned. Otherwise, if
** an error occurs, an SQLite error code is returned.
*/
int loadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave)
{
    int rc;                   /* Function return code */
    sqlite3 *pFile;           /* Database connection opened on zFilename */
    sqlite3_backup *pBackup;  /* Backup object used to copy data */
    sqlite3 *pTo;             /* Database to copy to (pFile or pInMemory) */
    sqlite3 *pFrom;           /* Database to copy from (pFile or pInMemory) */

    /* Open the database file identified by zFilename. Exit early if this fails
    ** for any reason. */
    rc = sqlite3_open(zFilename, &pFile);
    if( rc==SQLITE_OK )
    {
        /* If this is a 'load' operation (isSave==0), then data is copied
        ** from the database file just opened to database pInMemory.
        ** Otherwise, if this is a 'save' operation (isSave==1), then data
        ** is copied from pInMemory to pFile.  Set the variables pFrom and
        ** pTo accordingly. */
        pFrom = (isSave ? pInMemory : pFile);
        pTo   = (isSave ? pFile     : pInMemory);

        /* Set up the backup procedure to copy from the "main" database of
        ** connection pFile to the main database of connection pInMemory.
        ** If something goes wrong, pBackup will be set to NULL and an error
        ** code and message left in connection pTo.
        **
        ** If the backup object is successfully created, call backup_step()
        ** to copy data from pFile to pInMemory. Then call backup_finish()
        ** to release resources associated with the pBackup object.  If an
        ** error occurred, then an error code and message will be left in
        ** connection pTo. If no error occurred, then the error code belonging
        ** to pTo is set to SQLITE_OK.
        */
        pBackup = sqlite3_backup_init(pTo, "main", pFrom, "main");
        if( pBackup )
        {
            (void)sqlite3_backup_step(pBackup, -1);
            (void)sqlite3_backup_finish(pBackup);
        }
        rc = sqlite3_errcode(pTo);
    }

    /* Close the database connection opened on database file zFilename
    ** and return the result of this function. */
    (void)sqlite3_close(pFile);
    return rc;
}

int loadOrSaveDb_journal_in_memory(sqlite3 *pInMemory, const char *zFilename, int isSave)
{
    int rc;                   /* Function return code */
    sqlite3 *pFile;           /* Database connection opened on zFilename */
    sqlite3_backup *pBackup;  /* Backup object used to copy data */
    sqlite3 *pTo;             /* Database to copy to (pFile or pInMemory) */
    sqlite3 *pFrom;           /* Database to copy from (pFile or pInMemory) */

    /* Open the database file identified by zFilename. Exit early if this fails
    ** for any reason. */
    rc = sqlite3_open(zFilename, &pFile);
    if( rc==SQLITE_OK )
    {
        if( isSave )
        {
            sqlite3_exec(pFile, "PRAGMA journal_mode = MEMORY;", 0, 0, 0);
        }

        /* If this is a 'load' operation (isSave==0), then data is copied
        ** from the database file just opened to database pInMemory.
        ** Otherwise, if this is a 'save' operation (isSave==1), then data
        ** is copied from pInMemory to pFile.  Set the variables pFrom and
        ** pTo accordingly. */
        pFrom = (isSave ? pInMemory : pFile);
        pTo   = (isSave ? pFile     : pInMemory);

        /* Set up the backup procedure to copy from the "main" database of
        ** connection pFile to the main database of connection pInMemory.
        ** If something goes wrong, pBackup will be set to NULL and an error
        ** code and message left in connection pTo.
        **
        ** If the backup object is successfully created, call backup_step()
        ** to copy data from pFile to pInMemory. Then call backup_finish()
        ** to release resources associated with the pBackup object.  If an
        ** error occurred, then an error code and message will be left in
        ** connection pTo. If no error occurred, then the error code belonging
        ** to pTo is set to SQLITE_OK.
        */
        pBackup = sqlite3_backup_init(pTo, "main", pFrom, "main");
        if( pBackup )
        {
            (void)sqlite3_backup_step(pBackup, -1);
            (void)sqlite3_backup_finish(pBackup);
        }
        rc = sqlite3_errcode(pTo);
    }

    /* Close the database connection opened on database file zFilename
    ** and return the result of this function. */
    (void)sqlite3_close(pFile);
    return rc;
}
/*
** Perform an online backup of database pDb to the database file named
** by zFilename. This function copies 5 database pages from pDb to
** zFilename, then unlocks pDb and sleeps for 250 ms, then repeats the
** process until the entire database is backed up.
**
** The third argument passed to this function must be a pointer to a progress
** function. After each set of 5 pages is backed up, the progress function
** is invoked with two integer parameters: the number of pages left to
** copy, and the total number of pages in the source file. This information
** may be used, for example, to update a GUI progress bar.
**
** While this function is running, another thread may use the database pDb, or
** another process may access the underlying database file via a separate
** connection.
**
** If the backup process is successfully completed, SQLITE_OK is returned.
** Otherwise, if an error occurs, an SQLite error code is returned.
*/
int backupDb(
    sqlite3 *pDb,               /* Database to back up */
    const char *zFilename,      /* Name of file to back up to */
    void(*xProgress)(int, int)  /* Progress function to invoke */
)
{
    int rc;                     /* Function return code */
    sqlite3 *pFile;             /* Database connection opened on zFilename */
    sqlite3_backup *pBackup;    /* Backup handle used to copy data */

    backup_base_now = true;
    /* Open the database file identified by zFilename. */
    rc = sqlite3_open(zFilename, &pFile);
    if( rc==SQLITE_OK )
    {
        /* Open the sqlite3_backup object used to accomplish the transfer */
        pBackup = sqlite3_backup_init(pFile, "main", pDb, "main");

        if( pBackup )
        {
            /* Each iteration of this loop copies 5 database pages from database
            ** pDb to the backup database. If the return value of backup_step()
            ** indicates that there are still further pages to copy, sleep for
            ** 250 ms before repeating. */
            do
            {
                rc = sqlite3_backup_step(pBackup, 50);

                xProgress(
                    sqlite3_backup_remaining(pBackup),
                    sqlite3_backup_pagecount(pBackup)
                );

                if( rc==SQLITE_OK || rc==SQLITE_BUSY || rc==SQLITE_LOCKED )
                {
                    sqlite3_sleep(250);
                }
            }
            while( rc==SQLITE_OK || rc==SQLITE_BUSY || rc==SQLITE_LOCKED );

            /* Release resources allocated by backup_init(). */
            (void)sqlite3_backup_finish(pBackup);
        }
        rc = sqlite3_errcode(pFile);
    }

    /* Close the database connection opened on database file zFilename
    ** and return the result of this function. */
    (void)sqlite3_close(pFile);
    backup_base_now = false;
    return rc;
}

void progress(int r, int p)
{
    printf("progress: %d %d\n", r, p);
}

void* Thread1(void *arg)
{
    // backup базы в файл - копия
    //backupDb(s_db_handle_memory, full_path_base.c_str(), progress );
    loadOrSaveDb_journal_in_memory(s_db_handle_memory, full_path_base.c_str(), 1);

    pthread_exit(0); /* terminate the thread */
}

int my_url_callback(http_parser* parser, const char *at, size_t length)
{
  struct HttpData *d = (struct HttpData *) parser->data;
  d->url.append( at, length );

  return 0;
}

int my_body_callback(http_parser* parser, const char *at, size_t length)
{
  struct HttpData *d = (struct HttpData *) parser->data;
  d->body.append( at, length );

  return 0;
}

///---------------------------------------------------------------------------------------------
void msleep(unsigned int msec)
{
    struct timespec timeout0;
    struct timespec timeout1;
    struct timespec* tmp;
    struct timespec* t0 = &timeout0;
    struct timespec* t1 = &timeout1;

    t0->tv_sec = msec / 1000;
    t0->tv_nsec = (msec % 1000) * (1000 * 1000);

    while(nanosleep(t0, t1) == -1)
    {
        if(errno == EINTR)
        {
            tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        else
            return;
    }
    return;
}

///------------------------------------------------------------------------------------------------------
int send_all(int socket, const void *buffer, size_t length, int flags)
{
    ssize_t n;
    const char *p = (const char *)buffer;
    int count_wait = 0;

    while (length > 0)
    {
        n = send(socket, p, length, flags);
        if (n <= 0)
        {
            count_wait += 1;

            if( count_wait > 10 )
            {
                break;
            }

            msleep(100);
            continue;
        }
        p += n;
        length -= n;
    }
    return (n <= 0) ? -1 : 0;
}

std::string string_format(const std::string &fmt, ...)
{
    int size = 100;
    std::string str;
    va_list ap;
    while (1)
    {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.c_str(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size)
        {
            str.resize(n);
            //printf("\nresize %d\n", n);
            return str;
        }
        if (n > -1)
            size = n + 1;
        else
            size *= 2;
    }
    return str;
}

static inline std::string &ltrim(std::string &s) 
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) 
{
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) 
{
    return ltrim(rtrim(s));
}

vector<string> &split(const string &s, char delim, vector<string> &elems) 
{
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

vector<string> string_split(const string &s, char delim) 
{
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}

/*static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
	if (    tok->type == JSMN_STRING
	     && (int) strlen(s) == tok->end - tok->start
	     && strncmp(json + tok->start, s, tok->end - tok->start) == 0)
    {
		return 0;
	}
	return -1;
}*/

string escape_quotes(const string &before)
{
    string after;
    after.reserve(before.length() + 4);

    for (string::size_type i = 0; i < before.length(); ++i) 
    {
        switch (before[i]) 
        {
            case '"':
            case '\\':
                after += '\\';
                // Fall through.

            default:
                after += before[i];
        }
    }

    return after;
}

void printDateTime()
{
      char buffer[26];
      int millisec;
      struct tm* tm_info;
      struct timeval tv;

      gettimeofday(&tv, NULL);

      millisec = lrint(tv.tv_usec/1000.0); // Round to nearest millisec
      if (millisec>=1000) { // Allow for rounding up to nearest second
        millisec -=1000;
        tv.tv_sec++;
      }

      tm_info = localtime(&tv.tv_sec);

      strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);
      printf("%s.%03d\n", buffer, millisec);
}

///---------------------------------------------------------------------------------
void *server_rcv_client_worker(void *arg)
{
    int sock = *((int *) arg);
    int rc;

    free( arg );

    fd_set  rfds;
    struct timeval timeout;

    //for(int k = 0; k < 20; k++)
    {
        timeout.tv_sec = 12; // only 12 second for exec thread
        timeout.tv_usec = 0;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        rc = select(sock + 1, &rfds, NULL, NULL, &timeout);
        if (rc == 0) //timeout
        {
            //printf("timeout\n");
            //break;
            goto exit_thread;
        } // timeout

        if(rc < 0)
        {
            //break;
            goto exit_thread;
        } // error

        if(rc > 0)
        {
            if(FD_ISSET(sock, &rfds))
            {
                char send_str[2048] = "";
                int count_read = read(sock, &send_str, 2048);

                if( count_read < 0 )
                {
                    //break;
                    goto exit_thread;
                }
                else if( count_read > 0 )
                {
                    //char send_str[2048] = "";
                    HttpData _http_data;

                    http_parser *parser = (http_parser *)malloc(sizeof(http_parser));
                    http_parser_init(parser, HTTP_REQUEST); /* initialise parser */
                    parser->data = &_http_data;

                    http_parser_settings settings;
                    settings.on_url  = my_url_callback;
                    settings.on_body = my_body_callback;

                    int nparsed = http_parser_execute(parser, &settings, send_str, count_read);

                    if( nparsed != count_read )
                    {
                        free(parser);
                        //break;
                        goto exit_thread;
                    }

                    //printf("url : %s\n", _http_data.url.c_str());
                    //printf("body: %s\n", _http_data.body.c_str());
                    
                    if( parser->method == HTTP_GET )
                    {
                        //printf("method: GET\n");
                        //printf("_http_data.url: %s\n", _http_data.url.c_str() );
                    }
                    else if( parser->method == HTTP_POST )
                    {
                        if( _http_data.url.compare("/get_data_sos/") == 0 )
                        {
                            int uid_invitations = 0;
                            sscanf(_http_data.body.c_str(), "p=%d", &uid_invitations);

                            //printf("user_id: %d\n", uid_invitations);

                            if( uid_invitations <= 0 )
                            {
                                free(parser);
                                snprintf(send_str, 2048, "HTTP/1.0 500 Error\r\nConnection: close\r\n\r\nError: parameters\r\n");

                                send(sock, send_str, strlen(send_str), 0);
                                goto exit_thread;
                            } 
                            
                            char sql_c[255] = "";

                            char *zErrMsg = 0;
                            int rc = 0;
                            
                            snprintf(
                                    sql_c
                                    , 255
                                    , 
                                      "SELECT uid_from, nik_from, text, lat, lng FROM sos_signals WHERE uid_to=%d LIMIT 1;"
                                      "DELETE FROM sos_signals WHERE uid_to=%d;"
                                    
                                    , uid_invitations
                                    , uid_invitations);

                            struct buf* output_byffer = (struct buf*)buf_new();
                            buf_append_u8( output_byffer, '[' );

                            /* Execute SQL statement */
                            rc = sqlite3_exec((sqlite3*)s_db_handle_memory, sql_c, callback_select_data_array, (void*)output_byffer, &zErrMsg);
                            if( rc != SQLITE_OK )
                            {
                                string error_mess = "";
                                       error_mess.append(zErrMsg);

                                error_mess = escapeJsonString( error_mess );

                                string _res = string_format("{ \"error\":\"SQL error: %s\"}", error_mess.c_str() );
                                //fprintf(stderr, );
                                sqlite3_free(zErrMsg);

                                snprintf( send_str, 2048,
                                       "HTTP/1.1 404 SQL ERROR\r\n"
                                       "Content-Type: application/json\r\n"
                                       "Content-Length: %d\r\n"
                                       "\r\n"
                                       "%s",
                                       _res.length(), _res.c_str() );

                                buf_free( output_byffer );
                                free(parser);
                                send(sock, send_str, strlen(send_str), 0);
                                goto exit_thread;
                            }

                            if( output_byffer->ptr[ output_byffer->len - 1 ] == ',' )
                            {
                                output_byffer->ptr[ output_byffer->len - 1 ] = ']';
                            }
                            else
                            {
                                buf_append_u8( output_byffer, ']' );
                            }

                            snprintf(send_str, 2048, "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n", output_byffer->len);
                            free(parser);

                            send_all(sock, send_str, strlen(send_str), 0);
                            send_all(sock, output_byffer->ptr, output_byffer->len, 0);

                            buf_free( output_byffer );

                            //printDateTime();
                            goto exit_thread;
                        }
                        else if( _http_data.url.compare("/get_data_invitation/") == 0 )
                        {
                            int uid_invitations = 0;
                            sscanf(_http_data.body.c_str(), "p=%d", &uid_invitations);

                            //printf("user_id: %d\n", uid_invitations);

                            if( uid_invitations <= 0 )
                            {
                                free(parser);
                                snprintf(send_str, 2048, "HTTP/1.0 500 Error\r\nConnection: close\r\n\r\nError: parameters\r\n");

                                send(sock, send_str, strlen(send_str), 0);
                                goto exit_thread;
                            }

                            char sql_c[255] = "";
                            //snprintf(sql_c, 255, "DELETE FROM invitations_in_room WHERE uid_to=%d;", uid_invitations);

                            char *zErrMsg = 0;
                            int rc = 0;

                            /*rc = sqlite3_exec((sqlite3*)s_db_handle, sql_c, NULL, NULL, &zErrMsg);
                            if( rc != SQLITE_OK )
                            {
                                string error_mess = "";
                                       error_mess.append(zErrMsg);

                                error_mess = escapeJsonString( error_mess );

                                string _res = string_format("{ \"error\":\"SQL error: %s\"}", error_mess.c_str() );
                                //fprintf(stderr, );
                                sqlite3_free(zErrMsg);

                                snprintf(send_str, 2048, "HTTP/1.0 404 SQL ERROR\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s", _res.length(), _res.c_str());
                                free(parser);
                                send(sock, send_str, strlen(send_str), 0);
                                break;
                            }*/

                            snprintf(
                                    sql_c
                                    , 255
                                    , 
                                      "SELECT chid_to, nik_from, channel_name FROM invitation_on_channel WHERE uid_to=%d LIMIT 1;"
                                      "DELETE FROM invitation_on_channel WHERE uid_to=%d;"
                                    
                                    , uid_invitations
                                    , uid_invitations);

                            struct buf* output_byffer = (struct buf*)buf_new();
                            buf_append_u8( output_byffer, '[' );

                            /* Execute SQL statement */
                            rc = sqlite3_exec((sqlite3*)s_db_handle_memory, sql_c, callback_select_data_array, (void*)output_byffer, &zErrMsg);
                            if( rc != SQLITE_OK )
                            {
                                string error_mess = "";
                                       error_mess.append(zErrMsg);

                                error_mess = escapeJsonString( error_mess );

                                string _res = string_format("{ \"error\":\"SQL error: %s\"}", error_mess.c_str() );
                                //fprintf(stderr, );
                                sqlite3_free(zErrMsg);

                                snprintf( send_str, 2048,
                                       "HTTP/1.1 404 SQL ERROR\r\n"
                                       "Content-Type: application/json\r\n"
                                       "Content-Length: %d\r\n"
                                       "\r\n"
                                       "%s",
                                       _res.length(), _res.c_str() );

                                buf_free( output_byffer );
                                free(parser);
                                send(sock, send_str, strlen(send_str), 0);
                                goto exit_thread;
                            }

                            if( output_byffer->ptr[ output_byffer->len - 1 ] == ',' )
                            {
                                output_byffer->ptr[ output_byffer->len - 1 ] = ']';
                            }
                            else
                            {
                                buf_append_u8( output_byffer, ']' );
                            }

                            snprintf(send_str, 2048, "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n", output_byffer->len);
                            free(parser);

                            send_all(sock, send_str, strlen(send_str), 0);
                            send_all(sock, output_byffer->ptr, output_byffer->len, 0);

                            buf_free( output_byffer );

                            //printDateTime();
                            goto exit_thread;
                        }
                        else if( _http_data.url.compare("/search_address/") == 0 )
                        {
                            std::istringstream iss( _http_data.body );
                            // store query key/value pairs in a map
                            std::map<std::string, std::string> params;

                            std::string keyval, key, val;

                            while(std::getline(iss, keyval, '&')) // split each term
                            {
                                std::istringstream iss(keyval);

                                // split key/value pairs
                                if(std::getline(std::getline(iss, key, '='), val))
                                    params[key] = trim(val);
                            }

                            if(
                                    params.find("txt") == params.end()
                                 //|| params["key"].compare("882e620a-3d07-4722-961e-c5d3a5071877") != 0
                                 || params["txt"].length() == 0
                              )
                            {
                                snprintf(send_str, 2048, "HTTP/1.0 500 Error\r\nConnection: close\r\n\r\nError: command not found.\r\n");
                                send_all(sock, send_str, strlen(send_str), 0);
                                //break;
                                goto exit_thread;
                            }


                            string txt0 = urlDecode(params["txt"]);
                            string txt;

                            icu::UnicodeString someUString( txt0.c_str(), "UTF-8" );

                            icu::UnicodeString s_lower = someUString.toLower( "ru_RU" );
                            s_lower.toUTF8String(txt);

                            //-----------------------------------------------------------------
                            //printf("[%d] = %s\n", x, l0[x].c_str() );

                            txt.erase(std::remove(txt.begin(), txt.end(), '+'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '-'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '='), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '*'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '/'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '\\'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '}'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '{'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '('), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), ')'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '&'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '$'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '#'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '@'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '!'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '~'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '`'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), ':'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), ';'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '\''), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '"'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '<'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '>'), txt.end());
                            txt.erase(std::remove(txt.begin(), txt.end(), '?'), txt.end());

                            //printf("%s\n", params["txt"].c_str() );
                            //printf("%s\n", txt0.c_str() );
                            //printf("%s\n", txt.c_str() );

                            /*string num_house = "";
                            for(int p = txt.length() - 1; p >= 0; p--)
                            {

                            }*/

                            ///****************************************************************************
                            /*{
                                    string sql2 = string_format("SELECT * FROM search WHERE search = '%s' LIMIT 1", txt0.c_str());

                                    printf("+++ %s\n", sql2.c_str() );

                                    char *zErrMsg = 0;
                                    int rc = 0;

                                    struct buf* output_byffer = (struct buf*)buf_new();
                                    buf_append_u8( output_byffer, '[' );

                                    rc = sqlite3_exec((sqlite3*)s_db_handle_memory, sql2.c_str(), callback_select_data, (void*)output_byffer, &zErrMsg);
                                    if( rc != SQLITE_OK )
                                    {
                                        string error_mess = "";
                                               error_mess.append(zErrMsg);

                                        error_mess = escapeJsonString( error_mess );

                                        string _res = string_format("{ \"error\":\"SQL error: %s\"}", error_mess.c_str() );
                                        //fprintf(stderr, );
                                        sqlite3_free(zErrMsg);

                                        snprintf(send_str, 2048, "HTTP/1.0 404 SQL ERROR\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n%s", _res.length(), _res.c_str());
                                        free(parser);
                                        buf_free( output_byffer );
                                        send_all(sock, send_str, strlen(send_str), 0);
                                        break;
                                    }

                                    if( output_byffer->ptr[ output_byffer->len - 1 ] == ',' )
                                    {
                                        output_byffer->ptr[ output_byffer->len - 1 ] = ']';
                                    }
                                    else
                                    {
                                        buf_append_u8( output_byffer, ']' );
                                    }

                                    if( output_byffer->len > 2 )
                                    {
                                        snprintf(send_str, 2048, "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n", output_byffer->len);
                                        free(parser);

                                        send_all(sock, send_str, strlen(send_str), 0);
                                        send_all(sock, output_byffer->ptr, output_byffer->len, 0);

                                        buf_free( output_byffer );
                                        break;
                                    }
                            }*/
                            ///****************************************************************************

                            /*vector<string> like_list = string_split(txt, ' ');
                            vector<string> l0;

                            for(unsigned int x = 0; x < like_list.size(); x++)
                            {
                                string v5 = trim( like_list[x] );

                                if( v5.length() == 0 )
                                { continue; }

                                //printf("[%d] = %s\n", x, like_list[x].c_str() );
                                l0.push_back( string_format(
                                  "("
                                  " s1 LIKE '%%%s%%' "
                                  " OR s2 LIKE '%%%s%%' "
                                  " OR s3 LIKE '%%%s%%' "
                                  " OR s4 LIKE '%%%s%%' "
                                  " OR s5 LIKE '%%%s%%' "
                                  " OR s6 LIKE '%%%s%%' "
                                  " OR s7 LIKE '%%%s%%' "
                                  " OR s8 LIKE '%%%s%%' "
                                  " OR s9 LIKE '%%%s%%' "
                                  " OR s10 LIKE '%%%s%%' "
                                  " OR s11 LIKE '%%%s%%' "
                                  " OR s12 LIKE '%%%s%%' "
                                  " OR s13 LIKE '%%%s%%' "
                                  " OR s14 LIKE '%%%s%%' "
                                  " OR s15 LIKE '%%%s%%' "
                                 " )"
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                     , v5.c_str()
                                  ) );
                            }*/

                            /*for(unsigned int x = 0; x < like_list.size(); x++)
                            {
                                printf("[%d] = %s\n", x, l0[x].c_str() );
                            }*/

                            /*string _and    = " AND ";
                            string join_s  = join(l0, _and);*/

                            //string query = string_format( "SELECT * FROM search WHERE full = '%s' OR %s LIMIT 5", txt0.c_str(), join_s.c_str() );
                            //string query = string_format( "SELECT full FROM search WHERE %s LIMIT 5", join_s.c_str() );
                            string query = string_format( "SELECT full FROM search WHERE s MATCH '%s' LIMIT 5", txt.c_str() );

                            //printDateTime();

                            //printf("")

                            printf("query: %s\n", query.c_str() );
                            //free(parser);
                            //break;

                                    char *zErrMsg = 0;
                                    int rc = 0;

                                    struct buf* output_byffer = (struct buf*)buf_new();
                                    buf_append_u8( output_byffer, '[' );

                                    /* Execute SQL statement */
                                    rc = sqlite3_exec((sqlite3*)s_db_handle_memory, query.c_str(), callback_select_data, (void*)output_byffer, &zErrMsg);
                                    if( rc != SQLITE_OK )
                                    {
                                        string error_mess = "";
                                               error_mess.append(zErrMsg);

                                        error_mess = escapeJsonString( error_mess );

                                        string _res = string_format("{ \"error\":\"SQL error: %s\"}", error_mess.c_str() );
                                        //fprintf(stderr, );
                                        sqlite3_free(zErrMsg);

                                        snprintf(send_str, 2048, "HTTP/1.0 404 SQL ERROR\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n%s", _res.length(), _res.c_str());
                                        free(parser);
                                        buf_free( output_byffer );
                                        send_all(sock, send_str, strlen(send_str), 0);
                                        //break;
                                        goto exit_thread;
                                    }

                                    if( output_byffer->ptr[ output_byffer->len - 1 ] == ',' )
                                    {
                                        output_byffer->ptr[ output_byffer->len - 1 ] = ']';
                                    }
                                    else
                                    {
                                        buf_append_u8( output_byffer, ']' );
                                    }

                                    /*if( output_byffer->len == 2 )
                                    {
                                        string txt_all;
                                        txt_all.append( (const char*) output_byffer->ptr, output_byffer->len );

                                        printf("%s\n", txt_all.c_str() );
                                    }*/

                                    //snprintf(send_str, 2048, "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: http://localhost:81/\r\nContent-Length: %d\r\n\r\n", output_byffer->len);
                                    snprintf(send_str, 2048, "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin: http://192.168.77.71:81/\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n", output_byffer->len);
                                    free(parser);

                                    send_all(sock, send_str, strlen(send_str), 0);
                                    send_all(sock, output_byffer->ptr, output_byffer->len, 0);

                                    buf_free( output_byffer );

                                    //printDateTime();
                                    //break;
                                    goto exit_thread;



                            /*snprintf(send_str, 2048, "HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n" );
                            send_all(sock, send_str, strlen(send_str), 0);
                            send_all(sock, txt.c_str(), txt.length(), 0);

                            free(parser);
                            break;*/
                        }
                        /// список регионов
                        else if( _http_data.url.compare("/get_regions_list/") == 0 )
                        {
                            std::istringstream iss( _http_data.body );
                            // store query key/value pairs in a map
                            std::map<std::string, std::string> params;

                            std::string keyval, key, val;

                            while(std::getline(iss, keyval, '&')) // split each term
                            {
                                std::istringstream iss(keyval);

                                // split key/value pairs
                                if(std::getline(std::getline(iss, key, '='), val))
                                    params[key] = val;
                            }

                            /*for(std::map<std::string, std::string>::iterator _it = params.begin(); _it != params.end(); _it++ )
                            {
                                printf("param: '%s'='%s'\n", _it->first.c_str(), _it->second.c_str() );
                            }*/

                            /*if(
                                    params.find("key") == params.end()
                                 || params["key"].compare("882e620a-3d07-4722-961e-c5d3a5071877") != 0
                              )
                            {
                                snprintf(send_str, 2048, "HTTP/1.0 500 Error\r\nConnection: close\r\n\r\nError: command not found.\r\n");
                                send_all(sock, send_str, strlen(send_str), 0);
                                break;
                            }*/

                            char sql_c[255] = "";
                            char *zErrMsg = 0;
                            int rc = 0;

                            snprintf(sql_c, 255, "SELECT * FROM regions");

                            struct buf* output_byffer = (struct buf*)buf_new();
                            buf_append_u8( output_byffer, '[' );

                            /* Execute SQL statement */
                            rc = sqlite3_exec((sqlite3*)s_db_handle_memory, sql_c, callback_select_data, (void*)output_byffer, &zErrMsg);
                            if( rc != SQLITE_OK )
                            {
                                string error_mess = "";
                                       error_mess.append(zErrMsg);

                                error_mess = escapeJsonString( error_mess );

                                string _res = string_format("{ \"error\":\"SQL error: %s\"}", error_mess.c_str() );
                                //fprintf(stderr, );
                                sqlite3_free(zErrMsg);

                                snprintf( send_str, 2048,
                                       "HTTP/1.1 404 SQL ERROR\r\n"
                                       "Content-Type: application/json\r\n"
                                       "Content-Length: %ld\r\n"
                                       "\r\n"
                                       "%s",
                                       _res.length(), _res.c_str() );

                                buf_free( output_byffer );
                                free(parser);
                                send_all(sock, send_str, strlen(send_str), 0);
                                //break;
                                goto exit_thread;
                            }

                            if( output_byffer->ptr[ output_byffer->len - 1 ] == ',' )
                            {
                                output_byffer->ptr[ output_byffer->len - 1 ] = ']';
                            }
                            else
                            {
                                buf_append_u8( output_byffer, ']' );
                            }

                            //printf(">> %s\n", output_byffer->ptr);

                            snprintf(send_str, 2048, "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n", output_byffer->len);

                            send_all(sock, send_str, strlen(send_str), 0);
                            send_all(sock, output_byffer->ptr, output_byffer->len, 0);

                            free(parser);
                            buf_free( output_byffer );
                            //break;
                            goto exit_thread;
                        }
                        else if( _http_data.url.compare("/search/") == 0 ) /// список регионов
                        {
                            std::istringstream iss( _http_data.body );
                            // store query key/value pairs in a map
                            std::map<std::string, std::string> params;

                            std::string keyval, key, val;

                            while(std::getline(iss, keyval, '&')) // split each term
                            {
                                std::istringstream iss(keyval);

                                // split key/value pairs
                                if(std::getline(std::getline(iss, key, '='), val))
                                    params[key] = val;
                            }

                            /*for(std::map<std::string, std::string>::iterator _it = params.begin(); _it != params.end(); _it++ )
                            {
                                printf("param: '%s'='%s'\n", _it->first.c_str(), _it->second.c_str() );
                            }*/

                            if(
                                    params.find("key") == params.end()
                                 || params.find("v") == params.end()
                                 || params.find("rf") == params.end()
                                 //|| params["key"].compare("882e620a-3d07-4722-961e-c5d3a5071877") != 0
                                 || params["v"].length() == 0
                                 || params["rf"].length() == 0
                              )
                            {
                                snprintf(send_str, 2048, "HTTP/1.0 500 Error\r\nConnection: close\r\n\r\nError: command not found.\r\n");
                                send_all(sock, send_str, strlen(send_str), 0);
                                //break;
                                goto exit_thread;
                            }

                            char sql_c[2048] = "";
                            char *zErrMsg = 0;
                            int rc = 0;

                            string region_first_level = escape_quotes( params["rf"] );
                            string v                  = escape_quotes( params["v"] );

                            vector<string> all_words = string_split(v, ' ');
                            string add_query = "";

                            for(unsigned t0 = 0, len0 = all_words.size(); t0 < len0; t0++)
                            {
                                string tmp_s = trim( all_words[t0] );

                                if( tmp_s.length() > 0 )
                                {
                                    add_query.append( string_format(" AND name_search LIKE '%%%s%%' ", tmp_s.c_str() ) );
                                }
                            }

                            snprintf(sql_c, 2048, "select aoid,full_reverse as name,house_list from places where aoguid_firts_level = '%s' %s LIMIT 30", region_first_level.c_str(), add_query.c_str());

                            //printf("%s\n", sql_c);
                            //break;

                            struct buf* output_byffer = (struct buf*)buf_new();
                            buf_append_u8( output_byffer, '[' );

                            /* Execute SQL statement */
                            rc = sqlite3_exec((sqlite3*)s_db_handle_memory, sql_c, callback_select_data, (void*)output_byffer, &zErrMsg);
                            if( rc != SQLITE_OK )
                            {
                                string error_mess = "";
                                       error_mess.append(zErrMsg);

                                error_mess = escapeJsonString( error_mess );

                                string _res = string_format("{ \"error\":\"SQL error: %s\"}", error_mess.c_str() );
                                //fprintf(stderr, );
                                sqlite3_free(zErrMsg);

                                snprintf( send_str, 2048,
                                       "HTTP/1.1 404 SQL ERROR\r\n"
                                       "Content-Type: application/json\r\n"
                                       "Content-Length: %ld\r\n"
                                       "\r\n"
                                       "%s",
                                       _res.length(), _res.c_str() );

                                buf_free( output_byffer );
                                free(parser);
                                send_all(sock, send_str, strlen(send_str), 0);
                                //break;
                                goto exit_thread;
                            }

                            if( output_byffer->ptr[ output_byffer->len - 1 ] == ',' )
                            {
                                output_byffer->ptr[ output_byffer->len - 1 ] = ']';
                            }
                            else
                            {
                                buf_append_u8( output_byffer, ']' );
                            }

                            printf(">> %s\n", output_byffer->ptr);

                            snprintf(send_str, 2048, "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n", output_byffer->len);

                            send_all(sock, send_str, strlen(send_str), 0);
                            send_all(sock, output_byffer->ptr, output_byffer->len, 0);
                            
                            free(parser);
                            buf_free( output_byffer );
                            //break;
                            goto exit_thread;
                        }
                        else
                        {
                            std::istringstream iss( _http_data.body );
                            // store query key/value pairs in a map
                            std::map<std::string, std::string> params;

                            std::string keyval, key, val;

                            while(std::getline(iss, keyval, '&')) // split each term
                            {
                                std::istringstream iss(keyval);

                                // split key/value pairs
                                if(std::getline(std::getline(iss, key, '='), val))
                                    params[key] = val;
                            }

                            /*for(std::map<std::string, std::string>::iterator _it = params.begin(); _it != params.end(); _it++ )
                            {
                                printf("param: '%s'='%s'\n", _it->first.c_str(), _it->second.c_str() );
                            }*/

                            if(
                                    params.find("cmd") != params.end()
                                 && params.find("value") != params.end()
                                 && params["cmd"].compare("query") == 0
                                 && params["value"].length() > 0
                              )
                            {
                                //printf("OK\n");

                                string sql = params["value"];

                                sql = ltrim(sql);

                                bool select_query = ( strncasecmp(sql.c_str(), "select ", 7) == 0 )?true:false;
                                
                                //printf("exec q: %s\n", sql.c_str());

                                if( ! select_query )
                                {
                                    //static const struct mg_str ok_request2 = MG_MK_STR("OK2");
                                    char *zErrMsg = 0;
                                    int rc;

                                    rc = sqlite3_exec((sqlite3*)s_db_handle_memory, sql.c_str(), NULL, NULL, &zErrMsg);
                                    if( rc != SQLITE_OK )
                                    {
                                        string error_mess = "";
                                               error_mess.append(zErrMsg);

                                        error_mess = escapeJsonString( error_mess );

                                        string _res = string_format("{ \"error\":\"SQL error: %s\"}", error_mess.c_str() );
                                        //fprintf(stderr, );
                                        sqlite3_free(zErrMsg);

                                        snprintf(send_str, 2048, "HTTP/1.0 404 SQL ERROR\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n%s", _res.length(), _res.c_str());
                                        free(parser);
                                        send_all(sock, send_str, strlen(send_str), 0);
                                        //break;
                                        goto exit_thread;
                                    }

                                    /// синхронное изменение данных
                                    if(
                                            params.find("sync_exec") != params.end()
                                         && params["sync_exec"].length() > 0
                                         && params["sync_exec"].compare("yes") == 0
                                      )
                                    {
                                        rc = sqlite3_exec((sqlite3*)s_db_handle, sql.c_str(), NULL, NULL, &zErrMsg);
                                        if( rc != SQLITE_OK )
                                        {
                                                string error_mess = "";
                                                       error_mess.append(zErrMsg);

                                                error_mess = escapeJsonString( error_mess );

                                                string _res = string_format("{ \"error\":\"SQL error: %s\"}", error_mess.c_str() );
                                                //fprintf(stderr, );
                                                sqlite3_free(zErrMsg);

                                                snprintf(send_str, 2048, "HTTP/1.0 404 SQL ERROR\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n%s", _res.length(), _res.c_str());
                                                free(parser);
                                                send_all(sock, send_str, strlen(send_str), 0);
                                                //break;
                                                goto exit_thread;
                                        }
                                    }

                                    snprintf(send_str, 2048, "HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n" );
                                    free(parser);
                                    send_all(sock, send_str, strlen(send_str), 0);
                                    //break;
                                    goto exit_thread;
                                }
                                else
                                {
                                    char *zErrMsg = 0;
                                    int rc = 0;

                                    struct buf* output_byffer = (struct buf*)buf_new();
                                    buf_append_u8( output_byffer, '[' );
                                    
                                    /* Execute SQL statement */
                                    rc = sqlite3_exec((sqlite3*)s_db_handle_memory, sql.c_str(), callback_select_data, (void*)output_byffer, &zErrMsg);
                                    if( rc != SQLITE_OK )
                                    {
                                        string error_mess = "";
                                               error_mess.append(zErrMsg);

                                        error_mess = escapeJsonString( error_mess );

                                        string _res = string_format("{ \"error\":\"SQL error: %s\"}", error_mess.c_str() );
                                        //fprintf(stderr, );
                                        sqlite3_free(zErrMsg);

                                        snprintf(send_str, 2048, "HTTP/1.0 404 SQL ERROR\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n%s", _res.length(), _res.c_str());
                                        free(parser);
                                        buf_free( output_byffer );
                                        send_all(sock, send_str, strlen(send_str), 0);
                                        //break;
                                        goto exit_thread;
                                    }

                                    if( output_byffer->ptr[ output_byffer->len - 1 ] == ',' )
                                    {
                                        output_byffer->ptr[ output_byffer->len - 1 ] = ']';
                                    }
                                    else
                                    {
                                        buf_append_u8( output_byffer, ']' );
                                    }

                                    snprintf(send_str, 2048, "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n", output_byffer->len);
                                    free(parser);

                                    send_all(sock, send_str, strlen(send_str), 0);
                                    send_all(sock, output_byffer->ptr, output_byffer->len, 0);

                                    buf_free( output_byffer );
                                    //break;
                                    goto exit_thread;

                                }// ---
                            }
                            else if(
                                    params.find("cmd") != params.end()
                                 && params["cmd"].compare("meminfo") == 0
                              )
                            {
                                unsigned long currRealMem = 0, peakRealMem = 0, currVirtMem = 0, peakVirtMem = 0;

                                // stores each word in status file
                                char buffer[1024] = "";

                                // linux file contains this-process info
                                FILE* file = fopen("/proc/self/status", "r");

                                // read the entire file
                                while (fscanf(file, " %1023s", buffer) == 1) 
                                {
                                    if (strcmp(buffer, "VmRSS:") == 0) 
                                    {
                                        fscanf(file, " %d", &currRealMem);
                                        currRealMem *= 1024;
                                    }
                                    
                                    if (strcmp(buffer, "VmHWM:") == 0) 
                                    {
                                        fscanf(file, " %d", &peakRealMem);
                                        peakRealMem *= 1024;
                                    }
                                    
                                    if (strcmp(buffer, "VmSize:") == 0) 
                                    {
                                        fscanf(file, " %d", &currVirtMem);
                                        currVirtMem *= 1024;
                                    }
                                    
                                    if (strcmp(buffer, "VmPeak:") == 0) 
                                    {
                                        fscanf(file, " %d", &peakVirtMem);
                                        peakVirtMem *= 1024;
                                    }
                                }
                                
                                fclose(file);
                                
                                free(parser);
                                
                                char buffHumanSize[50] = "";
                                
                                string json_res = string_format("{currRealMem:%lu,peakRealMem:%lu,currVirtMem:%lu,peakVirtMem:%lu,str_currRealMem:'%s',str_peakRealMem:'%s',str_currVirtMem:'%s',str_peakVirtMem:'%s'}"
                                            , currRealMem
                                            , peakRealMem
                                            , currVirtMem
                                            , peakVirtMem
                                        
                                            , pretty_bytes( buffHumanSize, 50, currRealMem )
                                            , pretty_bytes( buffHumanSize, 50, peakRealMem )
                                            , pretty_bytes( buffHumanSize, 50, currVirtMem )
                                            , pretty_bytes( buffHumanSize, 50, currRealMem )
                                        );
                                
                                snprintf(send_str, 2048, "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %lu\r\n\r\n", json_res.length());
                                send_all(sock, send_str, strlen(send_str), 0);
                                send_all(sock, json_res.c_str(), json_res.length(), 0);
                                
                                //break;
                                goto exit_thread;
                            }
                        }

                    } // --if

                    snprintf(send_str, 2048, "HTTP/1.0 500 Error\r\nConnection: close\r\n\r\nError: command not found.\r\n");
                    free(parser);
                    //buf_free( output_byffer );
                    send_all(sock, send_str, strlen(send_str), 0);
                    //break;
                    goto exit_thread;
                }

            }
        } /// -------------
    }

    exit_thread:
    
        FD_CLR(sock, &rfds);

        shutdown( sock, SHUT_RDWR );
        close(sock);

        pthread_exit(0);          /* terminate the thread */
}
///---------------------------------------------------------------------------
void *db_open(const char *db_path)
{
    sqlite3 *db = NULL;
    if( sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL) == SQLITE_OK )
    {
        //sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS kv(key PRIMARY KEY, val);", 0, 0, 0);
    }
    return db;
}

void db_close(sqlite3 **db_handle)
{
    if (db_handle != NULL && *db_handle != NULL)
    {
        sqlite3_close( *db_handle);
        *db_handle = NULL;
    }
}
///-----------------------------------------------------------------------------
void fn_exit()
{
    printf("exit function\n");

    if( s_db_handle != NULL )
    {
        db_close(&s_db_handle);
    }

    if( s_db_handle_memory != NULL )
    {
        db_close(&s_db_handle_memory);
    }
}

bool file_exists(string path)
{
    FILE *file = fopen(path.c_str(), "r");
    if(file != NULL)
    {
        fclose(file);
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    atexit( fn_exit );

    struct sockaddr_in stSockAddr;
    int sock_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP );
    struct sockaddr_in remoteaddr; // client address
    int addlen;
    int reuse_addr = 1;
    pthread_attr_t pattr;
    pthread_t child;
    //ClearClientThread *clear_client_thread;

    printDateTime();

    //--------------------------------------------------------------
    char path_save[512] = "";
    char *p;
    string s = "";

    if( ! (p = strrchr(argv[0], '/')) )
    {
        getcwd(abs_exe_path, sizeof(abs_exe_path));
    }
    else
    {
        *p = '\0';
        getcwd(path_save, sizeof(path_save));
        chdir(argv[0]);
        getcwd(abs_exe_path, sizeof(abs_exe_path));
        chdir(path_save);
    }

    //snprintf( abs_exe_path, 512, "%s", argv[0] );

    printf( "abs_exe_path: %s\n", abs_exe_path );

    string path_to_file_config = string_format("%s/config.ini", abs_exe_path);
    INIReader reader( path_to_file_config );

    int THREAD1_PORT = reader.GetInteger("params", "port", 8787);
    string listener  = reader.Get("params", "listener", "*");
    string db_file_name  = reader.Get("params", "db_file_name", "*");
    
    //printf( "db_file_name: %s\n", db_file_name.c_str() );
    printf( "listener: %s\n", listener.c_str() );

    if( listener.compare("*") != 0 )
    {
        not_limit_listener = false;

        vector<string> list_ips = string_split( listener, ',' );

        for(uint32_t k = 0; k < list_ips.size(); k++)
        {
            listener_list_ips.push_back( trim( list_ips[k] ) );
        }
    }

    printf("%s\n", path_to_file_config.c_str());
    printf("not_limit_listener: %d\n", not_limit_listener ? 1 : 0 );
    printf("Server port: %d\n", THREAD1_PORT);
    
    signal(SIGPIPE, SIG_IGN);
    //-------------------------------------------------------------

    if ( sock_server == -1 )
    {
        perror( "ошибка при создании сокета" );
        exit( EXIT_FAILURE );
    }

    memset( &stSockAddr, 0, sizeof( stSockAddr ) );

    stSockAddr.sin_family       = AF_INET;
    stSockAddr.sin_port         = htons( THREAD1_PORT );
    stSockAddr.sin_addr.s_addr  = INADDR_ANY;

    if ( bind( sock_server, ( const sockaddr* )&stSockAddr, sizeof( stSockAddr ) ) == -1 )
    {
        perror( "error bind 3" );
        close( sock_server );
        exit( EXIT_FAILURE );
    }

    setsockopt(sock_server, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int));

    if ( listen( sock_server, 1024 ) == -1 )
    {
        perror( "error listen" );
        close( sock_server );
        exit( EXIT_FAILURE );
    }

    pthread_attr_init(&pattr);
    pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_DETACHED);
    

    full_path_base.append( abs_exe_path );
    full_path_base.append( "/" );
    full_path_base.append( db_file_name );
    
    if( ! file_exists( full_path_base ) )
    {
        perror("file database not found\n");
        close( sock_server );
        exit( EXIT_FAILURE );
    }

    printf("path2: %s\n", full_path_base.c_str() );

    if ((s_db_handle = (sqlite3 *)db_open(full_path_base.c_str())) == NULL)
    {
        fprintf(stderr, "Cannot open DB [%s]\n", full_path_base.c_str());
        exit(EXIT_FAILURE);
    }

    // memory database
    if ((s_db_handle_memory = (sqlite3 *)db_open(s_db_memory)) == NULL)
    {
        fprintf(stderr, "Cannot memory open DB [%s]\n", s_db_memory);
        exit(EXIT_FAILURE);
    }

    sqlite3_exec(s_db_handle, "PRAGMA page_size = 150;", 0, 0, 0);
    //sqlite3_exec(s_db_handle, "PRAGMA journal_mode = MEMORY;", 0, 0, 0);

    sqlite3_exec(s_db_handle_memory, "PRAGMA page_size = 150;", 0, 0, 0);
    sqlite3_exec(s_db_handle_memory, "PRAGMA journal_mode = OFF;", 0, 0, 0);

    printf("=========== loading... ========================\n");

    /// загрузить из файла в память
    if( file_exists(full_path_base.c_str()) )
    {
        loadOrSaveDb(s_db_handle_memory, full_path_base.c_str(), 0);
    }

    sqlite3_enable_load_extension(s_db_handle_memory, 1);
    sqlite3_load_extension(s_db_handle_memory, "/home/sk/server11/bin/Debug/libSqliteIcu.so", 0, 0);
    sqlite3_load_extension(s_db_handle_memory, "/home/sk/server11/bin/Debug/fts4.so", 0, 0);

    char IPv4[INET_ADDRSTRLEN];
    char IPv6[INET6_ADDRSTRLEN];
    bool found_ip = false;

    printf("=========== start work ========================\n");
    printf("server start listen...\n");

    while(true)
    {
        addlen = sizeof( remoteaddr );
        int socket_client = accept( sock_server, ( sockaddr* ) &remoteaddr, (socklen_t*) &addlen);

        if ( socket_client < 0 )
        {
            perror("error accept");
            close( socket_client );
            close( sock_server );
            exit( EXIT_FAILURE );
        }

        if( ! not_limit_listener )
        {
            found_ip = false;

            if(remoteaddr.sin_family == AF_INET)
            {
                struct sockaddr_in *addr_in = (struct sockaddr_in *)&remoteaddr;
                inet_ntop(AF_INET, &(addr_in->sin_addr), IPv4, INET_ADDRSTRLEN);

                for(uint32_t k = 0; k < listener_list_ips.size(); k++)
                {
                    if( listener_list_ips[k].compare(IPv4) == 0 )
                    {
                        found_ip = true;
                        break;
                    }
                }
            }
            else if(remoteaddr.sin_family == AF_INET6)
            {
                struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)&remoteaddr;
                inet_ntop(AF_INET6, &(addr_in6->sin6_addr), IPv6, INET6_ADDRSTRLEN);

                for(uint32_t k = 0; k < listener_list_ips.size(); k++)
                {
                    if( listener_list_ips[k].compare(IPv4) == 0 )
                    {
                        found_ip = true;
                        break;
                    }
                }
            }

            if( ! found_ip )
            {
                printf("error found_ip");
                
                shutdown( socket_client, SHUT_RDWR );
                close(socket_client);
                continue;
            }
        }

        int *arg = (int *) malloc(sizeof(*arg));
        if ( arg == NULL )
        {
            fprintf(stderr, "Couldn't allocate memory for thread arg.\n");
            exit(EXIT_FAILURE);
        }

        *arg = socket_client;

        time_t rawtime;
        struct tm * timeinfo;

        time ( &rawtime );
        timeinfo = localtime ( &rawtime );

        //printf("ok conect\n");
        //printf("selectserver: new connection from %s %s\n", inet_ntoa(remoteaddr.sin_addr), asctime (timeinfo));
        
        /* creat thred ... */
        pthread_create(&child, &pattr, server_rcv_client_worker, arg);

        //printf("\n++create thread\n");
    }

    db_close(&s_db_handle);
    db_close(&s_db_handle_memory);

    pthread_exit(0);          /* terminate the thread */

    return 0;
}
