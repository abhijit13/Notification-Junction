/*  
    Notification-Junction is an interface between multiple applications and multiple Notification Providers.
    Copyright (C) 2015      Navroop Kaur<watwanichitra@gmail.com>, 
    Shivani Marathe<maratheshivani94@gmail.com>, 
    Kaveri Sangale<sangale.kaveri9@gmail.com>
    All Rights Reserved.

    This program is free software; you can redistribute it and/or modify it under the terms of the 
    GNU General Public License as published by the Free Software Foundation; either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
    without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with this program; if not, write to the 
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* 
 *	The file contains the code for the Notification-Junction implementation 
 *	It has functions for 
 *	app registration	
 *	app unregistration
 *	np registration
 *	np unregistration 
 *	app_registraion thread routine
 *	app_unregistration thread 	
 *	np_registration thread 	
 *	np_unregistration thread routine
 *	app getnotify 
 *	app getnotify thead routine
 *	np getnotify thread routine
 *
 */

#include "nj.h"

np_dcll npList;						/* Head of global NP List */
FILE* logfd;						/* File pointer of log file */
app_dcll appList;					/* Head of global APP List */
int fd_pidnames;					/* FD of file used for storing PID filenames */
void *handle;						/* Handle of the function in shared library */
pthread_mutex_t app_list_mutex, np_list_mutex, getnotify_socket_mutex;
/*Mutexes to be used for synchronizing list*/

/* main code */
int main()
{	int ret;
	sigset_t mask;
	struct thread_args stat, app_reg, np_reg, app_unreg,np_unreg, app_getnotify;
	pthread_t tid_stat, tid_app_reg, tid_app_unreg, tid_np_reg, tid_np_unreg, tid_app_getnotify;
	atexit(nj_exit);
	if (!(logfd = fopen("LOGS", "a+"))) {
		perror("NJ.C   : not able to open  Log file\n");
		return 1;
	}


	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	signal(SIGINT, sigint_handler);

	/* first remove(unlink) the sockets if they already exist */
	unlink(StatSocket);
	unlink(AppRegSocket);
	unlink(AppUnRegSocket);
	unlink(NpRegSocket);
	unlink(NpUnRegSocket);
	unlink(AppGetnotifySocket);

	fd_pidnames = open(PIDFILE, O_CREAT | O_RDWR, 0777);

	/*Initialization of all mutexes */
	if (pthread_mutex_init(&app_list_mutex, NULL) != 0) {
		perror("NJ.C   : \n app_list mutex init failed\n");
		exit(EXIT_FAILURE);
	}
	if (pthread_mutex_init(&np_list_mutex, NULL) != 0) {
		perror("NJ.C   : \n np_list mutex init failed\n");
		exit(EXIT_FAILURE);
	}

	if (pthread_mutex_init(&getnotify_socket_mutex, NULL) != 0) {
		perror("NJ.C   : \n getnotify socket mutex init failed\n");
		exit(EXIT_FAILURE);
	}


	/*Initialize NP List */
	if(pthread_mutex_lock(&np_list_mutex) != 0) {
		perror("NJ.C: Np List mutex Lock failed :");
	};
	init_np(&npList);
	if(pthread_mutex_unlock(&np_list_mutex) != 0) {
		perror("NJ.C : Np list mutex unlock failed:");
	};	

	/*Initialize APP List */
	if(pthread_mutex_lock(&app_list_mutex) != 0 ) {
		perror("Nj.c Applist mutex Lock failed");
	}
	init_app(&appList);

	if(pthread_mutex_unlock(&app_list_mutex) != 0) {
		perror("Nj.c applist mutex unlock failed");
	}

	/* Create socket for stat*/

	/* stat.sock : ; */
	stat.sock = socket(AF_UNIX, SOCK_STREAM, 0);
	PRINTF("> %s %d main() :Socket stat created\n",__FILE__ , __LINE__);
	if (stat.sock < 0) {
		perror("opening stream socket");
		exit(EXIT_FAILURE);
	}
	stat.server.sun_family = AF_UNIX;
	strcpy(stat.server.sun_path, StatSocket);
	if (bind(stat.sock, (struct sockaddr *)&stat.server, sizeof(struct sockaddr_un))) {
		perror("binding stream socket");
		exit(EXIT_FAILURE);
	}

	/* create socket for application registration*/
	app_reg.sock = socket(AF_UNIX, SOCK_STREAM, 0);
	PRINTF("> %s %d main() : Socket app_reg created\n",__FILE__ , __LINE__);
	if (app_reg.sock < 0) {
		perror("NJ.C   : opening stream socket");
		exit(EXIT_FAILURE);
	}
	app_reg.server.sun_family = AF_UNIX;
	strcpy(app_reg.server.sun_path, AppRegSocket);
	if (bind(app_reg.sock, (struct sockaddr *)&app_reg.server, sizeof(struct sockaddr_un))) {
		perror("NJ.C   : binding stream socket");
		exit(EXIT_FAILURE);
	}

	/* create socket for application unregistration */
	app_unreg.sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (app_unreg.sock < 0) {
		perror("NJ.C   : opening stream socket");
		exit(EXIT_FAILURE);
	}
	app_unreg.server.sun_family = AF_UNIX;
	strcpy(app_unreg.server.sun_path, AppUnRegSocket);

	if (bind(app_unreg.sock, (struct sockaddr *)&app_unreg.server, sizeof(struct sockaddr_un))) {
		perror("NJ.C   : binding stream socket");
		exit(EXIT_FAILURE);
	}

	/* socket for np registration */
	np_reg.sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (np_reg.sock < 0) {
		perror("NJ.C   : opening stream socket");
		exit(EXIT_FAILURE);
	}
	np_reg.server.sun_family = AF_UNIX;
	strcpy(np_reg.server.sun_path, NpRegSocket);
	if (bind(np_reg.sock, (struct sockaddr *)&np_reg.server, sizeof(struct sockaddr_un))) {
		perror("NJ.C   : binding stream socket");
		exit(EXIT_FAILURE);
	}

	/* socket for np unregistration */
	np_unreg.sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (np_unreg.sock < 0) {
		perror("NJ.C   : opening stream socket");
		exit(EXIT_FAILURE);
	}
	np_unreg.server.sun_family = AF_UNIX;
	strcpy(np_unreg.server.sun_path, NpUnRegSocket);
	if (bind(np_unreg.sock, (struct sockaddr *)&np_unreg.server, sizeof(struct sockaddr_un))) {
		perror("NJ.C   : binding stream socket");
		exit(EXIT_FAILURE);
	}

	/* create socket for app side getnotify */
	app_getnotify.sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (app_getnotify.sock < 0) {
		perror("NJ.C   : opening stream socket for getnotify");
		exit(EXIT_FAILURE);
	}
	app_getnotify.server.sun_family = AF_UNIX;
	strcpy(app_getnotify.server.sun_path, AppGetnotifySocket);
	if (bind(app_getnotify.sock, (struct sockaddr *)&app_getnotify.server, sizeof(struct sockaddr_un))) {
		perror("NJ.C   : binding stream socket");
		exit(EXIT_FAILURE);
	}

	/* fork threads to listen  and accept */
	if ((ret = pthread_create(&tid_stat, NULL, &print_stat, (void *)&stat)) == 0) {
		PRINTF("> %s %d main(): Pthread_Creation successful for stat\n",__FILE__ , __LINE__);
	}
	else {
		errno = ret;
		perror("Error in pthread_create for stat thread:");
	}

	if ((ret = pthread_create(&tid_app_reg, NULL, &app_reg_method, (void *)&app_reg)) == 0) {
		PRINTF(">%s %d main() : Pthread_Creation successful for app_reg_method \n",__FILE__ , __LINE__);
	}
	else {
		errno = ret;
		perror("Error in pthread_create for app_reg_method :");
	}
	if ((ret = pthread_create(&tid_app_unreg, NULL, &app_unreg_method, (void *)&app_unreg)) == 0) {
		PRINTF("> %s %d main()    : Pthread_Creation successful for app_unreg_method\n",__FILE__ , __LINE__);
	}
	else {
		errno = ret;
		perror("Error in pthread_create for app_unreg method:");
	}
	if ((ret = pthread_create(&tid_np_reg, NULL, &np_reg_method, (void *)&np_reg)) == 0) {
		PRINTF("> %s %d main() : Pthread_Creation successful np_reg_method\n",__FILE__ , __LINE__);
	}
	else {
		errno = ret;
		perror("Error in pthread_create for np_reg_method:");
	}
	if (( ret = pthread_create(&tid_np_unreg, NULL, &np_unreg_method, (void *)&np_unreg)) == 0) {
		PRINTF("> %s %d main() : Pthread_Creation successful np_unreg_method\n",__FILE__ , __LINE__);
	}
	else {
		errno = ret;
		perror("Error in pthread_create for np_unreg_mehtod :");
	}
	if (( ret = pthread_create(&tid_app_getnotify, NULL, &app_getnotify_method, (void *)&app_getnotify)) == 0) {
		PRINTF(">%s %d main(): Pthread_creation of getnotify thread successful app_getnotify_method\n",__FILE__ , __LINE__);
	}
	else {
		errno = ret;
		perror("Error in pthread_create for app_getnotify_method :");
	}

	/*Join all the threads */
	if((ret = pthread_join(tid_np_reg, NULL)) != 0 ) {
		errno = ret;
		perror("nj.c : Error in pthread_join joining Np Reg thread:");

	}
	if((ret = pthread_join(tid_np_unreg, NULL)) != 0) {
		errno = ret;
		perror("nj.c : Error in pthread_join  joing Np unreg thread");
	}
	if((ret = pthread_join(tid_app_reg, NULL)) != 0) {
		errno = ret;
		perror("Error in pthread_join joining app register thread");
	}
	if((ret = pthread_join(tid_app_unreg, NULL)) != 0 ) {
		errno = ret;
		perror("Error in pthread_join joining app Unregister thread");
	}

	if((ret = pthread_join(tid_app_getnotify, NULL)) != 0 ) {
		errno = ret;
		perror("Error in pthread_join joining app_getnotify thread");
	}


	exit(EXIT_SUCCESS);

}

/* method to read socket for statistics */
void *print_stat(void *arguments)
{
	struct thread_args *args = arguments;


	if(listen(args->sock, QLEN) != 0 ) {
		perror("Error in listening on stat socket");
	}
	for (;;) {
		args->msgsock = accept(args->sock, 0, 0);
		if (args->msgsock == -1)
			perror(" Error accept on stat socket");
		else {
			do {
				bzero(args->buf, sizeof(args->buf));
				if ((args->rval = read(args->msgsock, args->buf, 1024)) < 0)
					perror("reading stream message");
				else if (args->rval == 0) {
					PRINTF("\n>%s %d print_stat() :Printing Statistics :\n",__FILE__ , __LINE__);
					printStat();
				}
			} while (args->rval > 0);

		}
		close(args->msgsock);
	}

	close(args->sock);
	unlink(StatSocket);

	pthread_exit(NULL);
	return NULL;
}

/* Read the app_register socket; runs in the thread forked for the purpose and calls the appropriate function */
void *app_reg_method(void *arguments)
{
	struct thread_args *args = arguments;


	if ((listen(args->sock, QLEN)) != 0) {
		perror("Error in listening on appreg socket:");
	}
	int i = 0;
	/* Till the program runs */
	for (;;) {
		args->msgsock = accept(args->sock, 0, 0);
		if (args->msgsock == -1)
			perror("NJ.C   : Error in accept app reg socket");
		else
			/* For every argument that might be given */
			do {
				bzero(args->buf, sizeof(args->buf));
				if ((args->rval = read(args->msgsock, args->buf, 1024)) < 0)
					perror("NJ.C   : reading stream message");
				else if (args->rval == 0)
					PRINTF("> %s %d app_reg_method()   : Ending connection\n",__FILE__ , __LINE__);
				/*code to register application ie add entry in list */
				else {	
					i = register_app(args->buf);
					if (i < 0) {
						PRINTF("> %s %d app_reg_method(): Error in registering, try again\n",__FILE__ , __LINE__);
						perror("Error in app_reg_method() : ");
					}
					break;
				}
			} while (args->rval > 0);

		close(args->msgsock);
	}

	close(args->sock);
	unlink(AppRegSocket);

	pthread_exit(NULL);
	return NULL;
}

/* app unregister method that will run in thread forked for unregistration and call the appropriate function */
void *app_unreg_method(void *arguments)
{
	struct thread_args *args = arguments;
	int i = 0;

	if((listen(args->sock, QLEN)) != 0) {
		perror("Error in listening on app_unreg socket");
	}

	for (;;) {
		args->msgsock = accept(args->sock, 0, 0);
		if (args->msgsock == -1)
			perror("NJ.C   : Error in accept of app_unreg socket");
		else
			do {
				bzero(args->buf, sizeof(args->buf));
				if ((args->rval =
							read(args->msgsock, args->buf, 1024)) < 0)
					perror
						("NJ.C   : reading stream message");
				else if (args->rval == 0)
					PRINTF(">%s %d app_unreg_method()  : Ending connection\n",__FILE__ , __LINE__);
				/* Call the function for the list */
				else {
					i = unregister_app(args->buf);

					if (i < 0)	{
						PRINTF("> %s %d app_unreg_method(): Error in registering\n",__FILE__ , __LINE__);
						perror("Error in app_reg_method() : ");
					}					

					break;
				}
			} while (args->rval > 0);

		close(args->msgsock);
	}

	close(args->sock);
	unlink(AppUnRegSocket);

	pthread_exit(NULL);
	return NULL;
}

/*np register method that will run in thread forked for np registration*/
void *np_reg_method(void *arguments)
{
	struct thread_args *args = arguments;


	if((listen(args->sock, QLEN)) != 0) {
		perror("Error in listening on np_reg socket");
	}
	int i = 0;
	for (;;) {
		args->msgsock = accept(args->sock, 0, 0);
		if (args->msgsock == -1)
			perror("NJ.C   : Error in accept of np_reg socket");
		else
			do {
				bzero(args->buf, sizeof(args->buf));
				if ((args->rval =
							read(args->msgsock, args->buf, 1024)) < 0)
					perror
						("NJ.C   : reading stream message");
				else if (args->rval == 0)
					PRINTF("> %s %d np_reg_method(): Ending connection\n",__FILE__ , __LINE__);
				/*code to register np ie add entry in list */
				else {	
					i = register_np(args->buf);
					if (i < 0) {
						PRINTF(">%s %d np_reg_method(): Error in registering\n",__FILE__ , __LINE__);
						perror("Error in np_reg_method() : ");

					}
					break;
				}
			} while (args->rval > 0);

		close(args->msgsock);
	}

	close(args->sock);
	unlink(NpRegSocket);

	pthread_exit(NULL);
	return NULL;
}

/*np unregister method that will run in thread forked for np unregistration*/
void *np_unreg_method(void *arguments)
{
	struct thread_args *args = arguments;

	if((listen(args->sock, QLEN)) != 0) {
		perror("Error in listening on np Unreg socket");
	}
	int i = 0;
	for (;;) {
		args->msgsock = accept(args->sock, 0, 0);
		if (args->msgsock == -1)
			perror("NJ.C   : Error in accept of np unreg socket");
		else
			do {
				bzero(args->buf, sizeof(args->buf));
				if ((args->rval =
							read(args->msgsock, args->buf, 1024)) < 0)
					perror("NJ.C   : reading stream message");
				else if (args->rval == 0)
					PRINTF(">%s %d np_unreg_method(): Ending connection\n",__FILE__ , __LINE__);
				/*code to unregister NP entry in list */
				else {
					i = unregister_np(args->buf);

					if (i < 0) {
						PRINTF("> %s %d np_unreg_method(): Error in registering\n",__FILE__ , __LINE__);
						perror("Error in np_unreg_method() : ");
					}
					break;
				}
			} while (args->rval > 0);

		close(args->msgsock);
	}

	close(args->sock);
	unlink(NpUnRegSocket);

	pthread_exit(NULL);
	return NULL;
}

/* app get notify method that will run in thread for get notification */
void *app_getnotify_method(void *arguments)
{
	struct thread_args *args = arguments;
	struct proceed_getn_thread_args *sendargs;
	struct sockaddr_un server;
	int i = 0;
	int ret ;


	fprintf(logfd,"> %s %d app_getnotify_method(): In AppGetnotifySocket\n",__FILE__ , __LINE__);
	fprintf(logfd,"> %s %d AppGetNotifyMehod(): args -> msgsock - %d\n",__FILE__ , __LINE__, args->msgsock);

	if((listen(args->sock, QLEN)) != 0) {
		perror("Error in listening on app_getnotify socket");
	}

	server.sun_family = AF_UNIX;
	strcpy(server.sun_path, AppGetnotifySocket);
	pthread_t threadarr[APPLIMIT];

	for (;;) {
		args->msgsock = accept(args->sock, 0, 0);
		if (args->msgsock == -1)
			perror("NJ.C   : Error in accept of app_getnotify_socket");
		else
			do {
				if((pthread_mutex_lock(&getnotify_socket_mutex)) != 0 ) {
					perror("Error in lock getnotify_socket_mutex:");
				}
				if((sendargs = (struct proceed_getn_thread_args *)malloc(sizeof(struct proceed_getn_thread_args))) == NULL) {
					PRINTF("> %s %d app_getnotify_method(): malloc failed\n ",__FILE__, __LINE__);
					errno = ECANCELED;
					perror("Malloc failed");
				}

				bzero(args->buf, sizeof(args->buf));
				args->rval = read(args->msgsock, args->buf, 1024);
				strcpy(sendargs->buf, args->buf);
				fprintf(logfd, ">%s %d app_getnotify_method() : 1. buf is %s \n",__FILE__ , __LINE__,sendargs->buf);

				if (args->rval < 0) {
					perror("NJ.C   : reading stream message");

					if((pthread_mutex_unlock(&getnotify_socket_mutex)) != 0) {
						perror("Error in unlock getnotify_socket_mutex:");
					}
				}    
				else if (args->rval == 0) {
					PRINTF("> %s %d app_getnotify_method(): Ending connection\n",__FILE__ , __LINE__);
					if((pthread_mutex_unlock(&getnotify_socket_mutex)) != 0) {
						perror("Error in unlock getnotify_socket_mutex:");
					}
				}
				/* code to fork a thread per getnotify() request and send the arguments to the thread's action */
				else {
					PRINTF("> %s %d app_getnotify_method(): %d is i in ProcerdGetnotify \n",__FILE__ ,__LINE__,i);

					if ((ret = pthread_create(&threadarr[i++], NULL, &proceed_getnotify_method, (void *)sendargs)) == 0) {
						PRINTF("> %s %d app_getnotify_method(): Pthread_Creations for proceed_getnotify_method\n",__FILE__, __LINE__);
					}
					else {
						errno = ret;
						perror("Error in pthread_create creating thread for proceed_getnotify_method");
					}
					if((pthread_mutex_unlock(&getnotify_socket_mutex)) != 0) {
						perror("Error in unlock getnotify_socket_mutex:");
					}

					break;
				}

			}while(args->rval > 0);

		close(args->msgsock);
	}
	while (i >= 0) {

		if((ret = pthread_join(threadarr[i], NULL)) != 0) {
			errno = ret;
			perror("Error in pthread_join joining threads created by app_getnotify Method:");
		}
		i--;
	}

	close(args->sock);
	unlink(AppUnRegSocket);

	pthread_exit(NULL);
}

/*np get notify method that will run np get notify thread*/
void *np_getnotify_method(void *arguments)
{
	char args_send_copy[1024], args_send_copy_2[1024], rough[1024], r[1024], appname[64], np_name[64], one[512], library[256];
	char delimattr[3] = "##", delimval[3] = "::";
	char *error, *countkey, *filename;
	char **pointer;    	
	int j, filefd, al, count = -1, k, x;
	struct getnotify_thread_args *bargs, *args; 
	struct extr_key_val *temp, *m, *p;
	np_node *nptr;
	main_np_node *np_node;	
	int pid;
	if((pthread_mutex_lock(&app_list_mutex)) != 0) {
		perror("Error in lock app_list_mutex : ");
	}
	if((pthread_mutex_lock(&np_list_mutex)) != 0) {
		perror("Error in lock np_list_mutex:");
	}

	bargs = (struct getnotify_thread_args *)arguments;
	if((args = (struct getnotify_thread_args *)malloc(sizeof(struct getnotify_thread_args))) == NULL) {
		PRINTF("> %s %d np_getnotify_method() : malloc failed\n", __FILE__, __LINE__);
		errno = ECANCELED;
		perror("Malloc failed");
	}

	*args = *bargs;

	fprintf(logfd, "> %s %d np_getnotify_method : 8. buf is %s \n",__FILE__ , __LINE__, args->argssend);
	void (*getnotify) (struct getnotify_thread_args *);


	PRINTF("> %s %d np_getnotify_method(): started args->argssend is %s\n",__FILE__ , __LINE__, args->argssend);
	strcpy(rough, args->argssend);
	strcpy(r, rough);

	strcpy(args_send_copy_2, args->argssend);

	fprintf(logfd, "> %s %d np_getnotify_method():9. buf is %s \n",__FILE__ , __LINE__, args->argssend);
	filename = get_filename(args_send_copy_2);
	fprintf(logfd, "> %s %d np_getnotify_mehod():10. buf is %s \n",__FILE__ , __LINE__, args->argssend);

	force_logs();

	strtok(r, delimattr);
	j = strlen(r);
	strcpy(r, args->argssend);
	strcpy(r, &(r[j + 2]));
	strcpy(args->argssend, r);

	/* Extract app_name and np_name from the arguments */
	strcpy(appname, strtok(rough, delimattr));
	pid = atoi(appname);
	fprintf(logfd, "> %s %d np_getnotify_method():77. pid is %d \n",__FILE__ , __LINE__, pid);
	strcpy(one, strtok(NULL, delimattr));
	strtok(one, delimval);	
	strcpy(np_name, strtok(NULL, delimval));

	/* Get the reg, malloc space for a getn registration key_val  */

	nptr = get_reg(&appList, appname, np_name);

	if (nptr) {
		PRINTF("\n> %s %d np_getnotify_method(): RETURNED nptr->name = %s\n",__FILE__ , __LINE__, nptr->name);
	}

	else {
		PRINTF(">%s %d np_getnotify_method():Regitration not found---------\n\n",__FILE__ , __LINE__);
		perror("Registration doesn't exist - ");
		//and exit
		errno = ENODEV;
	}

	if((temp = (struct extr_key_val *)malloc(sizeof(struct extr_key_val))) == NULL) {
		PRINTF("> %s %d np_getnotify_method(): malloc failed \n", __FILE__, __LINE__);
		errno = ECANCELED;
		perror("Malloc failed");
		/* For every malloc() handling */
	}

	temp->next = NULL;

	/* In the registration in the trailing list, append to a list of key-value pairs for each getnotify() call */

	m = nptr->key_val_ptr;
	p = m;


	if (m == NULL) {
		nptr->key_val_ptr = temp;
	} else {
		while (m != NULL) {
			p = m; 
			m = m->next;
		}
		p->next = temp;
		temp->next = NULL;  
	}

	/* Compare it with the arguments given during ./np_register to check the consistency */

	extract_key_val(args->argssend, &pointer);
	np_node = search_np(&npList, np_name);
	k = compare_array(&(np_node->key_val_arr), &pointer);

	if(k == 0)
		;

	else {
		PRINTF(">%s %d np_getnotify_method() :Array not matched..\n",__FILE__ , __LINE__);	
		errno = EINVAL;
		return NULL;
	}

	/* Set the pointers in the trailing list and convert the key-value array into an NP accepted format */

	temp->key_val_arr = pointer;
	forward_convert(&(np_node->key_val_arr),&pointer, args->argssend);

	countkey = get_val_from_args(args->argssend, "count");
	count = atoi(extract_val(countkey));
	fprintf(logfd, ">%s %d np_getnotify_method() : count is %d! for buf %s!\n",__FILE__ , __LINE__, count, args->argssend);

	x = get_np_app_cnt(&npList, np_name);

	/* x = 0 for no registrations and x > 1 for existing registrations. When getnotify() is called for the first time (x = 1), the library needs to be opened. */

	strcpy(library, "./libnp");
	strcat(library, np_name);
	strcat(library, ".so");

	if (x == 1) {
		PRINTF(">%s %d np_getnotify_method(): Opening library.\n",__FILE__ , __LINE__);
		handle = dlopen(library, RTLD_LAZY);
		if (!handle) {
			perror("NJ.C   : Problem with dlopen :");
			exit(EXIT_FAILURE);
		}

		PRINTF("> %s %d np_getnotify_method (): dlopen successful\n",__FILE__ , __LINE__);

	}

	getnotify = dlsym(handle, "getnotify");
	if ((error = dlerror()) != NULL) {
		perror("NJ.C   : Error in dlsym()");
		exit(EXIT_FAILURE);
	}


	strcpy(args_send_copy, args->argssend);	

	/* For the number of times the same arguments have to be monitored;
	 *	Open the file to prevent delayed write induced errors,
	 *	call getnotify() from the NP library
	 *	copy the notification returned,
	 *	send signal to the application,
	 *	close the file
	 */
	for(;count != 0; count--) {

		if (!(filefd = open(filename, O_CREAT | O_APPEND | O_RDWR , 0777))) {
			perror("NJ.C   : not able to open file\n");
			return NULL;
		}

		fprintf(logfd, ">%s %d np_getnotify_method(): 11 %d. buf is %s \n",__FILE__ , __LINE__, count, args->argssend);

		force_logs();
		(*getnotify) (args);
		PRINTF(">%s %d np_getnotify_method(): Recd = %s\n",__FILE__ , __LINE__, args->argsrecv);

		al = write((int)filefd, args->argsrecv, strlen(args->argsrecv));
		if (al < 0)
			perror("NJ.C   : Fwrite failed");
		else
			PRINTF(">%s %d np_getnotify_method(): %s written  %d bytes WRITTEN\n",__FILE__ , __LINE__, filename, al);

		strcpy(args->argssend, args_send_copy);
		if (kill(pid, SIGUSR1) == -1) {
			perror("Error in kill :\n");
		}
		close(filefd);

	}

	PRINTF("> %s %d np_getnotify_method(): ARGSRECV IN THREAD HANDLER = %s\n",__FILE__ , __LINE__, args->argsrecv);
	strcpy(bargs->argsrecv, args->argsrecv);


	free(args);
	args = NULL;


	if((pthread_mutex_unlock(&np_list_mutex)) != 0) {
		perror("Error in unlock np_list_mutex : ");
	}
	if((pthread_mutex_unlock(&app_list_mutex)) != 0) {
		perror("Error in unlock of app list mutex:");
	}

	pthread_exit(NULL);

}

/* Find the app in the list. If it is found, for every np in its trailing list; visit the np_dcll and decrement its count. */
void dec_all_np_counts(app_dcll * appList, np_dcll * npList, char *app_name)
{

	app_node *ptrapp;
	np_node *ptrnp;
	int cnt;

	PRINTF("> %s %d dec_all_np_counts() :All nps deleted, since np_name was passed NULL\n",__FILE__ , __LINE__);
	ptrapp = search_app(appList, app_name);

	if (ptrapp != NULL) {	/* App found */

		cnt = ptrapp->np_count;
		ptrnp = ptrapp->np_list_head;
		while (cnt) {
			decr_np_app_cnt(npList, ptrnp->name);
			ptrnp = ptrnp->next;
			cnt--;
		}

	} else {

		PRINTF("> %s %d dec_all_np_counts():Error Asked to delete non-existent application\n",__FILE__ , __LINE__);
		perror("App doesn't exist - ");

	}

}

/* Action function of the getnotify() threads */
void *proceed_getnotify_method(void *arguments)
{
	char *received, *buf;
	struct proceed_getn_thread_args *temparguments, *args;
	char rough[1024];
	char spid[32];
	char choice;
	int j;	
	int len;
	char filename[64], filename1[64];


	temparguments = (struct proceed_getn_thread_args *)arguments;
	if((args = (struct proceed_getn_thread_args *)malloc(sizeof(struct proceed_getn_thread_args))) == NULL) {
		PRINTF("> %s %d proceed_getnotify_method(): malloc failed\n",__FILE__, __LINE__);
		errno = ECANCELED;
		perror("Malloc failed");

	}

	*args = *temparguments;
	fprintf(logfd, "> %s %d proceed_getnotify_method():2. buf is %s \n",__FILE__ , __LINE__, args->buf);


	force_logs();

	strcpy(rough, args->buf);

	if(args->buf == NULL)
		PRINTF("> %s %d proceed_getnotify_method(): args->buf is NULL\n\n",__FILE__ , __LINE__);

	len = strlen(rough);
	choice = rough[len - 1];

	/* Extract pid */
	strcpy(spid, strtok(rough, "##"));
	for(j = 0; j < len ; j++) {
		if(rough[j] == '#')
			if(rough[j+1] == '#')
				break;
	}

	j = strlen(rough);

	strcpy(filename, "./");
	strcat(filename, spid);
	strcpy(filename1, spid);
	strcat(filename, ".txt");
	strcat(filename1, ".txt");
	strcat(filename1, "\n");

	fprintf(logfd, "> %s %d proceed_getnotify_method :3. buf is %s \n",__FILE__ , __LINE__, args->buf);  
	force_logs();

	if((buf = (char *)malloc(sizeof(char) * (strlen(args->buf) + 1 ))) == NULL ) {
		PRINTF("> %s %d proceed_getnotify_method(): malloc failed\n", __FILE__, __LINE__);
		errno = ECANCELED;
		perror("Malloc failed");
	}
	strcpy(buf, args->buf);

	/* Subsequent calls to return the notification to "received" */
	received = getnotify_app(buf);

	while(received == NULL) {
		pthread_exit(NULL);
	}

	fprintf(logfd, "> %s %d proceed_getnotify_method() :4. Notification received from getnotify_app is %s \n",__FILE__ , __LINE__, received); 
	force_logs();

	if (choice == NONBLOCKING) {
		force_logs();
		strcat(received, "\n");
		write((int)fd_pidnames, filename1, strlen(filename1));
		PRINTF("> %s %d proceed_getnotify_method(): PID filename is written successfully.....\n",__FILE__ , __LINE__);

	}

	if (choice == BLOCKING) {
		strcat(received, "\n");
		write((int)fd_pidnames, filename1, strlen(filename1));
		PRINTF("> %s %d proceed_getnotify_method(): PID filename is written successfully.....\n",__FILE__ , __LINE__);
	}

	free(received);
	received = NULL;
	free(temparguments);
	temparguments = NULL;
	free(args);
	args = NULL;

	pthread_exit(NULL);
}

/* Given key::val, get the key */
char* extract_key(char *key_val) {
	char *key, *ptr;
	char temp[128];

	strcpy(temp, key_val);
	ptr = strtok(temp, "::");
	if((key = (char *)malloc(sizeof(char) * (strlen(ptr) + 1))) == NULL) {
		PRINTF("> %s %d extract_key() : malloc failed \n", __FILE__, __LINE__);
		errno = ECANCELED;
		perror("Malloc failed");

	}
	strcpy(key, ptr);
	return key;
}

/* Given key::val, extract the val */
char* extract_val(char *key_val) {

	char *ptr, *val;
	char temp[128] ;

	strcpy(temp, key_val);
	ptr = strtok(temp, "::");
	ptr = (key_val + strlen(ptr) + 2);
	if(!ptr) {
		PRINTF(">%s %d extract_val:RETURNED NULL",__FILE__ , __LINE__);
		errno = ENODEV;
		return NULL;
	}
	if((val = (char *)malloc(sizeof(char) * (strlen(ptr) + 1))) == NULL) {
		PRINTF("> %s %d extract_val(): malloc failed\n", __FILE__, __LINE__);
		errno = ECANCELED;
		perror("Malloc failed");

	}
	strcpy(val, ptr);
	return val;
}

/* Compare keys in two arrays to check their validity, assume that keys given during ./np_register are correct */
int compare_array(char *** np_key_val_arr, char *** getn_key_val_arr) {
	char **one, **two, *key_one, *key_two;
	int found = 0;

	one = *getn_key_val_arr;
	while(*one != NULL) {
		found = 0;
		key_one = extract_key(*one);
		two = *np_key_val_arr;
		while(*two != NULL) {
			key_two = extract_key(*two);
			if(!(strcmp(key_one, key_two))) {
				found = 1;
				break;
			}
			two++;
		}
		if(found == 0) {
			PRINTF("> %s %d compare_array():ERROR NP cannot process key %s\n",__FILE__ , __LINE__, key_one);
			errno = EINVAL;
			free(key_one);
			free(key_two);
			return -1;
		}
		one++;
	}
	free(key_one);
	free(key_two);
	return 0;
}

/* Convert arguments from a char** array into a string */
void forward_convert(char ***np_key_val_arr,char ***getn_key_val_arr , char * fillit) {
	char **one, **two, *key_one, *key_two;
	int found = 0;
	char ret_string[512];

	strcpy(ret_string,"\0");
	two = *np_key_val_arr;
	while(*two != NULL) {
		found = 0;
		key_two = extract_key(*two);
		one = *getn_key_val_arr;
		while(*one != NULL) {
			key_one = extract_key(*one);
			if(!(strcmp(key_one, key_two))) {
				strcat(ret_string, *one);
				strcat(ret_string, "##");
				found = 1;
				break;
			}
			one++;
		}
		if(found == 0) {
			strcat(ret_string, *two);
			strcat(ret_string, "##");
		}
		two++;
	}
	strcpy(fillit, ret_string);
	fillit[(strlen(fillit)) - 2] = '\0';
	return;
}

/* Given an argument string and a key, get the key::value corresponding to the key */
char* get_val_from_args(char *usage, char* key) {   
	char *occ, keycopy[128], *retstr, localkeyval[256];
	int i = 0;

	strcpy(keycopy, key);
	strcat(keycopy, "::");
	occ = strstr(usage, keycopy);
	while(*occ != '#') {
		localkeyval[i++] = *occ;
		occ++;    
	}
	localkeyval[i] = '\0';

	if((retstr = (char *)malloc(sizeof(char) * i)) == NULL) {
		PRINTF("> %s %d get_val_from_args() : malloc failed ", __FILE__, __LINE__);
	}

	strcpy(retstr, localkeyval);
	return retstr;
}

/* From an argument, extract thepid and make a filename from it */    
char *get_filename(char *argsbuf) {

	char rough[1024], *retstr;
	char spid[32], filename[64];

	strcpy(rough, argsbuf);
	strcpy(spid, strtok(rough, "##"));
	strcpy(filename, "./");
	strcat(filename, spid);
	strcat(filename, ".txt");
	PRINTF("> %s %d get_filename(): Filename:%s\n\n",__FILE__ , __LINE__, filename);
	if((retstr = (char*)malloc(sizeof(char) * (strlen(filename) + 1))) == NULL) {
		PRINTF("> %s %d get_filename(): malloc failed\n",__FILE__, __LINE__);
		errno = ECANCELED;
		perror("Malloc failed");

	}
	strcpy(retstr, filename);
	return retstr;
}

/* Function to forcefully write to log file */
void force_logs(void) {
	fclose(logfd);
	if (!(logfd = fopen(LOGS, "a+"))) {
		perror("NJ.C   : not able to open  Log file\n");
	}
	return;
}

/* Function To Print Statistics Of Nj */
void printStat()
{
	if((pthread_mutex_lock(&np_list_mutex)) != 0) {
		perror("Error in lock of np_list_mutex:");
	}
	print_np(&npList);
	if((pthread_mutex_unlock(&np_list_mutex)) != 0) {
		perror("Errror in unlock of np_list_mutex:");
	}
	if((pthread_mutex_lock(&app_list_mutex)) != 0) {
		perror("Errror in lock of app_list_mutex");
	}
	print_app(&appList);
	if((pthread_mutex_unlock(&app_list_mutex)) != 0) {
		perror("Error in unlock of app_list_mutex");
	}
}

/* Signal Handler Of Nj */
void sigint_handler(int signum)
{
	char buf2[64];
	PRINTF("> %s %d sigint_handler() : In sigIntHandlere\n", __FILE__, __LINE__);
	if (signum == SIGINT) {				/* Handling SIGINT signal to remove PID files */
		close(fd_pidnames);
		fd_pidnames = open(PIDFILE, O_CREAT | O_RDWR, 0777);
		FILE *pidnames = fdopen(fd_pidnames, "r");
		/* While there are PIDs of applications, remove them one by one */
		while (fgets(buf2, sizeof(buf2), pidnames)) {
			buf2[strlen(buf2) - 1] = '\0';
			unlink(buf2);
			PRINTF("> %s %d sigint_handler() : %s removed\n",__FILE__,__LINE__, buf2);
		}
		unlink(PIDFILE);
		PRINTF("> %s %d sigint_handler() : File_PIDS.txt removed\n", __FILE__, __LINE__);
		exit(EXIT_SUCCESS);
	}
}

/*To Register App With Nj*/
int register_app(char *buff)
{
	char app_name[32], np_name[32], delim[3] = "::";
	char *s;
	int retval;

	strcpy(app_name, strtok(buff, delim));
	s = strtok(NULL, delim);
	if (s != NULL)
		strcpy(np_name, s);

	if(pthread_mutex_lock(&app_list_mutex) != 0) {
		perror("NJ.C: app List mutex Lock failed :");
	};
	if(pthread_mutex_lock(&np_list_mutex) != 0) {
		perror("NJ.C: Np List mutex Lock failed :");
	};
	if(np_name != NULL) {
		/*Check if NP provided is registered or not */
		if (search_np(&npList, np_name) == NULL) {				/* NP is not registerd */
			perror("Np not registered - ");
			PRINTF("> %s %d register_app(): Np not registered. Register NP first.\n", __FILE__ , __LINE__);
			errno = ENODEV;
			return -1;
		}

		/*Check if app is already registered */
		if (search_app(&appList, app_name) == NULL) {				/* Registering app for the first time */
			add_app_node(&appList, app_name);
			PRINTF("> %s %d register_app(): Added for the first time\n", __FILE__ , __LINE__);
			add_np_to_app(&appList, app_name, np_name);                               
			incr_np_app_cnt(&npList, np_name);
		}
		else {									/* Adding NP registration to already */
			/* existing App */
			retval = search_reg(&appList, app_name, np_name);
			if (retval == -1) {
				PRINTF("> %s %d register_app() : EXISTING REGISTRATION\n", __FILE__ , __LINE__);
				perror("Registration exists, printing from perror - ");
				errno = EEXIST;
				return -2;
			}
			else {	 
				add_np_to_app(&appList, app_name, np_name);                               
				incr_np_app_cnt(&npList, np_name);
			}
		}

	}
	else {
		add_app_node(&appList, app_name);
		PRINTF("> %s %d register_app() :Only app is added successfully.\n", __FILE__ , __LINE__);
	}


	if(pthread_mutex_unlock(&np_list_mutex) != 0) {
		perror("NJ.C: Np List mutex unLock failed :");
	}
	if(pthread_mutex_unlock(&app_list_mutex) != 0) {
		perror("NJ.C: app List mutex Lock failed :");
	}

	return 1;
}

/*Function To Unregister An App*/
int unregister_app(char *buff)
{
	char app_name[32], np_name[32], delim[3] = "::";
	char *np_ptr;
	int val;

	strcpy(app_name, strtok(buff, delim));
	np_ptr = strtok(NULL, delim);

	/* If there is an np's name given */
	if (np_ptr != NULL) {
		strcpy(np_name, np_ptr);

	}

	if(pthread_mutex_lock(&app_list_mutex) != 0) {
		perror("NJ.C: app List mutex Lock failed :");
	};
	if(pthread_mutex_lock(&np_list_mutex) != 0) {
		perror("NJ.C: Np List mutex Lock failed :");
	};
	/* Only appname is given */
	if (np_ptr == NULL) {
		PRINTF("> %s %d unregister_app() :np_name == NULL case in unregister app\n", __FILE__ , __LINE__);
		/* Check is the application exists; if it does, delete it */
		if(search_app(&appList, app_name) != NULL) {
			del_app(&appList, app_name);		
			dec_all_np_counts(&appList, &npList, app_name);
			PRINTF("> %s %d unregister_app():Unregistration done\n", __FILE__, __LINE__);
		}
		/* If the application doesn't exist */
		else {
			PRINTF("> %s %d unregister_app(): App not found\n", __FILE__, __LINE__);

			perror("App not found, printing from perror - ");
			errno = ENODEV;
			return -3;
		}

	}

	/* Given appname::npname */
	else {
		/* If the registration exists, delete it */
		if ((val = search_reg(&appList, app_name, np_name)) == -1) {
			PRINTF("> %s %d unregister_app(): REGISTRATION FOUND.\n", __FILE__ , __LINE__);
			del_np_from_app(&appList, app_name, np_name);
			decr_np_app_cnt(&npList, np_name);
		}
		else if(val == 0) {
			PRINTF("> %s %d unregister_app(): Invalid argument to app_unregister : Registration not found\n", __FILE__, __LINE__);
			perror("App not found, printing from perror - ");
		}

	}


	if(pthread_mutex_unlock(&np_list_mutex) != 0) {
		perror("NJ.C: Np List mutex unLock failed :");
	}	
	if(pthread_mutex_unlock(&app_list_mutex) != 0) {
		perror("NJ.C: app List mutex unLock failed :");
	}

	return 1;
}

/*function to register an np*/
int register_np(char *buff)
{
	char np_name[32], * usage, delimusage[3] = "==";
	char *s;
	char **keyVal;
	main_np_node *new;
	usage = (char * ) malloc (sizeof(char) * 512);
	strcpy(np_name, strtok(buff, delimusage));
	s = strtok(NULL, delimusage);

	if (s == NULL) {
		PRINTF("> %s %d register_np():Enter usage\nExiting\n", __FILE__, __LINE__);
		return -1;
	}

	strcpy(usage, s);
	extract_key_val(usage, &keyVal);


	if(pthread_mutex_lock(&np_list_mutex) != 0) {
		perror("NJ.C: Np List mutex Lock failed :");
	};	


	new = search_np(&npList, np_name);					/* Search for already existing registration */

	if(new != NULL) {
		del_np(&npList, np_name);					/* Delete already existing registration to modify */ 											/* parameters */
	}

	add_np(&npList, np_name, usage, &keyVal);
	print_np(&npList);


	if(pthread_mutex_unlock(&np_list_mutex) != 0) {
		perror("NJ.C: Np List mutex unLock failed :");
	};
	return 1;
}

void extract_key_val(char *usage, char ***keyVal)
{
	int cnt = 0, i = 0;
	char copyusage[512];
	char *ptr;


	strcpy(copyusage, usage);
	cnt = count_args(copyusage, "::");					/* Counting no. of arguments */

	if((*keyVal = (char **)malloc((cnt + 1) * sizeof(char *))) == NULL) {
		PRINTF(" >%s %d extract_key_val() : malloc failed", __FILE__, __LINE__);
		errno = ECANCELED;
		perror("Malloc failed");

	}
	ptr = strtok(copyusage, "##");

	if(((*keyVal)[i] = (char *)malloc(sizeof(char) * (strlen(ptr) + 1))) == NULL) {
		PRINTF("> %s %d extract_key_val() : malloc failed ", __FILE__, __LINE__);
		errno = ECANCELED;
		perror("Malloc failed");

	}

	strcpy((*keyVal)[i], ptr);

	/* Extract key::value pairs and save them in keyVal */
	for (i = 1; i < cnt; i++) {
		ptr = strtok(NULL, "##");
		if (!ptr) {
			break;
		}
		if (((*keyVal)[i] = (char *)malloc(sizeof(char) * 128)) == NULL) {
			PRINTF("> %s %d extract_key_val(): Error in malloc", __FILE__, __LINE__);
			errno = ECANCELED;
			perror("Malloc failed");
		}
		strcpy((*keyVal)[i], ptr);
	}
	(*keyVal)[i] = NULL;
	return;
}

/* Count number of key::value pairs in the argument */
int count_args(char *myString, char *delim) {
	int count = 0;
	const char *tmp = myString;

	while ((tmp = (strstr(tmp, delim)))) {
		count++;
		tmp++;
	}
	return count;
}

/*function to unregister an np*/
int unregister_np(char *buff)
{

	printf("in unregister\n");

	char *np_name = (char *) malloc (sizeof(char) * 32);
	char delim[3] = "::";
	int app_cnt;
	app_node *aptr = appList.head;
	np_node *p, *q;

	strcpy(np_name, strtok(buff, delim));

	if(pthread_mutex_lock(&app_list_mutex) != 0) {
		perror("NJ.C: app List mutex Lock failed :");
	};
	if(pthread_mutex_lock(&np_list_mutex) != 0) {
		perror("NJ.C: Np List mutex Lock failed :");
	};
	app_cnt = appList.count;

	printf("Looking1\n");

	/* Check if the NP exists */    
	if(search_np(&npList, np_name) != NULL) {
		/* For every application it is registered with */
		while(app_cnt != 0) {
			/* Search every application's trailing list for a registration with that NP; remove that registration and reduce that application's count by one */			
			
			printf("Looking\n");
			
			if(search_reg(&appList, aptr->data, np_name) == -1) {
				p = aptr->np_list_head;

				while(p != NULL) {
					q = p;

					if(!strcmp(p->name, np_name)) {	
						PRINTF("> %s %d unregister_np():Np node found : %s\n", __FILE__ , __LINE__, np_name);
						break;	
					}

					p = p->next;
				}
				if(aptr->np_count == 1) {
					aptr->np_list_head = NULL;
					free(p);
					p = NULL;
					aptr->np_count--;
				}
				else if(p == q) {						/* If node to be deleted is the first node */
					q = p->next;
					aptr->np_list_head = q;
					free(p);
					p = NULL;
					aptr->np_count--;
				}

				else {
					q->next = p->next;
					free(p);
					p = NULL;
					aptr->np_count--;
				}
			}
			aptr = aptr->next;
			app_cnt--;	   	
		}
		printf("here\n");
		del_np(&npList, np_name);

	}
	else {
		PRINTF(">%s %d unregister_np(): Np not registerd.\n", __FILE__ , __LINE__);
		perror("Not found NP - ");
		errno = ENODEV;
		return -1;
	}

	print_np(&npList);
	print_app(&appList);

	if(pthread_mutex_unlock(&np_list_mutex) != 0) {
		perror("NJ.C: Np List mutex unLock failed :");
	}
	if(pthread_mutex_lock(&app_list_mutex) != 0) {
		perror("NJ.C: app List mutex unLock failed :");
	}
	return 1;
}

/*Function to get notification for app*/
char *getnotify_app(char *buff)
{	int ret;
	char *notification;
	struct getnotify_thread_args *arguments;
	pthread_t tid_app_np_gn;

	fprintf(logfd, "6. buf is %s \n", buff);
	force_logs();

	if((notification = (char *)malloc(1024 * sizeof(char))) == NULL) {
		PRINTF("> %s %d getnotify_app(): malloc failed",__FILE__,__LINE__);
		errno = ECANCELED;
		perror("Malloc failed");

	}
	if((arguments = (struct getnotify_thread_args *)malloc(sizeof(struct getnotify_thread_args))) == NULL) {
		PRINTF("> %s %d getnotify_app() : malloc failed ", __FILE__, __LINE__);
		errno = ECANCELED;
		perror("Malloc failed");

	}
	strcpy(arguments->argssend, buff);


	/* Fork thread to invoke the NP's code with the arguments given in the structure 'arguments', which contains argssend and argsrecv */	fprintf(logfd, "7. buf is %s \n", arguments->argssend);

	force_logs();
	if ((ret = pthread_create(&tid_app_np_gn, NULL, &np_getnotify_method,(void *)arguments) == 0)) {
		PRINTF(" > %s %d getnotify_app() :Pthread_Creation successful for getnotify\n", __FILE__ , __LINE__);
	}
	else {
		errno = ret;
		perror("Error in pthread_create for np_getnotify_method:");
	}

	if(( ret = pthread_join(tid_app_np_gn, NULL)) != 0) {
		perror("Error in joining threads pthread_join created by  getnotify_app(proceed_getnotify_method):");
	}

	strcpy(notification, arguments->argsrecv);

	free(buff);
	buff = NULL;

	free(arguments);
	arguments = NULL;

	return notification;	/*it will be callers responsibility to free the malloced notification string */
}

/* When NJ finally exits */
void nj_exit(void) {
	PRINTF(">%s %d nj_exit() : NJ exiting \n",__FILE__, __LINE__);
	int ret;
	/*pthread_cancel(tid_stat);
	  pthread_cancel(tid_app_reg);
	  pthread_cancel(tid_app_unreg);
	  pthread_cancel(tid_np_reg); 
	  pthread_cancel(tid_np_unreg);
	  pthread_cancel(tid_app_getnotify);
	 */
	 
	 /*
	if(pthread_mutex_trylock(&app_list_mutex) == EBUSY) {
		if(pthread_mutex_unlock(&app_list_mutex) != 0) {
			PRINTF("> %s %d nj_exit(): Error in unlocking app_list_mutex\n", __FILE__, __LINE__);
		}		
	}
	if(pthread_mutex_trylock(&np_list_mutex) == EBUSY) {
		if(pthread_mutex_unlock(&np_list_mutex) != 0) {
			PRINTF("> %s %d nj_exit(): Error in unlocking Np_list_mutex\n", __FILE__, __LINE__);
		}		
	}
	if((pthread_mutex_lock(&app_list_mutex)) != 0) {
		perror("Error acquiring lock for app list mutex:");
	}
	//empty_app_list(&appList);
	if((pthread_mutex_unlock(&app_list_mutex)) != 0) {
		perror("Error unlocking appList mutex:");
	}


	if((pthread_mutex_lock(&np_list_mutex)) != 0) {
	  perror("Error acquiring npList mutex:");
	  }
	  //empty_np_list(&npList);	
	  if((pthread_mutex_unlock(&np_list_mutex)) != 0) {
	  perror("Error unlocking npList mutex:");
	  }

	if((ret = pthread_mutex_destroy(&app_list_mutex)) != 0) {
		errno = ret;
		perror("Error in destroying app_list_mutex:");
	}
	if((ret = pthread_mutex_destroy(&np_list_mutex)) != 0) {
		errno = ret;
		perror("Error in destroying np_list_mutex:");
	}
	if((ret = pthread_mutex_destroy(&getnotify_socket_mutex)) != 0) {
		errno = ret;
		perror("Error in destroying getnotify_socket_mutex");
	}
*/



	/* For every np, delete its registration with the app first */
	int i = appList.count;
	int j = npList.count;
	PRINTF("> %s %d nj_exit(): %d app count \t %d np_count \n",__FILE__, __LINE__,i, j);
	struct app_node *temp, *temp1;
	struct main_np_node *temp2,*temp3;

	temp = appList.head;
	
	temp2 = npList.head;
	/*
	while(temp2 != NULL) {
		unregister_np(temp2->data);
		temp2 = temp2->next;
	}

	printf("> %s %d empty_np_list : Np list count is %d ",__FILE__, __LINE__, npList.count);
	*/
	
	printf("temp2->data = %s\n", temp2->data);

	
	for(; i != 0;i--) {
		temp1 = temp->next;
		unregister_app(temp->data);
		PRINTF("> %s %d nj_exit(): %d app_count \n",__FILE__,__LINE__,appList.count);
		temp = temp1;
	}
	
	printf(">%s %d empty_app_list : App List deleted completely  %d \n ",__FILE__ , __LINE__, appList.count);
	
	
	print_app(&appList);
	
	print_np(&npList);

	
	for(; j != 0;j--) {
		
		printf("IN NP LOOP, before delete Unregistering %s \n", temp2->data);
		temp3 = temp2->next;
		
		temp2->app_count = 0;
	
		unregister_np(temp2->data);
		
		printf("Deleting\n");
		
		temp2 = temp3;
		
		printf("next\n");
	
	}
	
	
	print_app(&appList);
	
	print_np(&npList);
	
	if((ret = pthread_mutex_destroy(&app_list_mutex)) != 0) {
		errno = ret;
		perror("Error in destroying app_list_mutex:");
	}
	if((ret = pthread_mutex_destroy(&np_list_mutex)) != 0) {
		errno = ret;
		perror("Error in destroying np_list_mutex:");
	}
	if((ret = pthread_mutex_destroy(&getnotify_socket_mutex)) != 0) {
		errno = ret;
		perror("Error in destroying getnotify_socket_mutex");
	}


}

