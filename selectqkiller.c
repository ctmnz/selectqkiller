#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <glib.h>


typedef struct 
{
 gchar *config_host;
 gchar *config_user;
 gchar *config_pass;
 gchar *config_logfilepath;
 int *config_max_query_time;
} Settings; 

int main() {

	// Configuration vars

	Settings *conf;
	GKeyFile *keyfile;
	GKeyFileFlags flags;
	GError *error = NULL;
	gsize length;


	
	keyfile = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
	if (!g_key_file_load_from_file (keyfile, "selectqkiller.cnf", flags, &error))
	 {
	  g_error(error->message);
	  return -1;
	 }
	conf = g_slice_new (Settings);
	conf->config_max_query_time = g_key_file_get_integer (keyfile, "Main", "config_max_query_time", NULL);
	conf->config_host = g_key_file_get_string (keyfile, "Mysql user config", "config_host", NULL);
	conf->config_user = g_key_file_get_string (keyfile, "Mysql user config", "config_user", NULL);
	conf->config_pass = g_key_file_get_string (keyfile, "Mysql user config", "config_pass", NULL);
	conf->config_logfilepath = g_key_file_get_string (keyfile, "Mysql user config", "config_logfilepath", NULL);



/// Inforation 

printf("Starting selectqkiller daemon for host: %s with max query time : %d ! Logfile path = %s " ,conf->config_host, conf->config_max_query_time, conf->config_logfilepath);

/// testing dump
/*
	printf("Config host = %s \n", conf->config_host);
        printf("Config user = %s \n", conf->config_user);
        printf("Config pass = %s \n", conf->config_pass);
        printf("Config logfilepath = %s \n", conf->config_logfilepath);
	printf("Config max query time = %d \n", conf->config_max_query_time);


	return 0;
*/
//////


	MYSQL *conn;
	MYSQL_RES *res;
	MYSQL_ROW row;

	// Connection for killing slow query
        MYSQL *conn2;
        MYSQL_RES *res2;
        MYSQL_ROW row2;


	char *server 	=  conf->config_host;
	char *user	= conf->config_user;
	char *password 	= conf->config_pass;
	char *database 	= "test";

	char *path = conf->config_logfilepath;

	int max_slow_time = conf->config_max_query_time;

	char *searching_select 	= "SELECT";
	char logged_slow_queries[100000] = "";
	char *log_helper;
	char killer_query[10000] = "";
	char log_message[1000] = "";

	int count_lsq = 0;
	int count_row_rec = 0;
	
	FILE *logfile;

	conn = mysql_init(NULL);
	conn2 = mysql_init(NULL);

	if(!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)) {
		fprintf(stderr, "%s\n", mysql_error(conn));
		exit(1);
	}

        if(!mysql_real_connect(conn2, server, user, password, database, 0, NULL, 0)) {
                fprintf(stderr, "%s\n", mysql_error(conn));
                exit(1);
        }


        pid_t pid, sid;
        
        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
                exit(EXIT_FAILURE);
        }
        /* If we got a good PID, then
           we can exit the parent process. */
        if (pid > 0) {
                exit(EXIT_SUCCESS);
        }

        /* Change the file mode mask */
        umask(0);
                
        /* Open any logs here */        
                
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
                /* Log the failure */
                exit(EXIT_FAILURE);
        }
        

        
        /* Change the current working directory */
        if ((chdir(path)) < 0) {
                /* Log the failure */
                exit(EXIT_FAILURE);
        }
        
        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        /* Daemon-specific initialization goes here */
        
        /* The Big Loop */
        while (1) {
           /* Do some task here ... */
                logfile=fopen("myslowlogfile.log","a");

                if (mysql_query(conn, "show full processlist")) {
                        fprintf(stderr, "%s\n", mysql_error(conn));
                        // exit(1);
			// FIX 
			strcat(log_message, "Cannot get processlist! \n" );
			fprintf(logfile, "%s", log_message);
			continue;
                }
		
                res = mysql_use_result(conn);
//              printf(" SELECT QUERIES:\n");
                while((row = mysql_fetch_row(res)) != NULL) {
                        if(row[7] != NULL) {
	                        int ii = strspn(row[7],searching_select);
                                if(ii==6) {
                                       int slow_time = atoi(row[5]);
                                       if(slow_time > max_slow_time ) {
                                                // printf("Write to logfile: (time:%s) query = %s \n",row[5], row[7]);

                                                strcat(logged_slow_queries, "PID: ");
                                                strcat(logged_slow_queries, row[0]);
                                                strcat(logged_slow_queries, " time: ");
                                                strcat(logged_slow_queries, row[5]);
                                                strcat(logged_slow_queries, " # database: ");
                                                strcat(logged_slow_queries, row[1]);
                                                strcat(logged_slow_queries, "@");
                                                strcat(logged_slow_queries, row[3]);
                                                strcat(logged_slow_queries, " # QUERY:");
                                                strcat(logged_slow_queries,  row[7]);
                                                strcat(logged_slow_queries, "\n");
                                                
						// kill query
						strcat(killer_query,"KILL ");
                                                strcat(killer_query, row[0]);
                                               // printf("query: %s \n",killer_query);

                                                if (mysql_query(conn2,killer_query)) {
                                                                fprintf(stderr, "%s\n", mysql_error(conn));
                                                                printf("Error while trying killing %s \n", row[0]);
                                               }

                                               res2 = mysql_use_result(conn2);
                                               strcpy(killer_query,"");
				      }
				}
			}
		}
		
                fprintf(logfile, "%s", logged_slow_queries);

                fclose(logfile);
                strcpy(logged_slow_queries,"");



           sleep(1); 
        }
   exit(EXIT_SUCCESS);
}

