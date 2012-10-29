/**
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Set of functions to gather system information for the jack setup wizard.
 * 
 * TODO: Test for rt prio availability
 *
 * @author Florian Faber, faber@faberman.de
 *
 **/

/** maximum number of groups a user can be a member of **/
#define MAX_GROUPS 100

#include <fcntl.h>

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <grp.h>

#include <sched.h>
#include <string.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <stdio.h>
#include <errno.h>

#include "systemtest.h"

/**
 * This function checks for the existence of known frequency scaling mechanisms 
 * in this system by testing for the availability of scaling governors/
 *
 * @returns 0 if the system has no frequency scaling capabilities non-0 otherwise.
 **/
int system_has_frequencyscaling() {
  int fd;

  fd = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors", O_RDONLY);

  if (-1==fd) {
    return 0;
  }

  (void) close(fd);

  return 1;
}


static int read_string(char* filename, char* buf, size_t buflen) {
  int fd;
  ssize_t r=-1;

  memset (buf, 0, buflen);

  fd = open (filename, O_RDONLY);
  if (-1<fd) {
    r = read (fd, buf, buflen-1);
    (void) close(fd);
    
    if (-1==r) {
      fprintf(stderr, "Error while reading \"%s\": %s\n", filename, strerror(errno));
      exit(EXIT_FAILURE);
    }
  }
  
  return (int) r;
}


static int read_int(char* filename, int* value) {
  char buf[20];

  if (0<read_string(filename, buf, 20)) {
		return (1==sscanf(buf, "%d", value));
  }

  return 0;
}


/**
 * This function determines wether any CPU core uses a variable clock speed if frequency 
 * scaling is available. If the governor for all cores is either "powersave" or
 * "performance", the CPU frequency can be assumed to be static. This is also the case
 * if scaling_min_freq and scaling_max_freq are set to the same value.
 *
 * @returns 0 if system doesn't use frequency scaling at the moment, non-0 otherwise
 **/
int system_uses_frequencyscaling() {
  int cpu=0, done=0, min, max;
  char filename[256], buf[256];

  while (!done) {
          (void) snprintf(filename, 256, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", cpu);
          if (0<read_string(filename, buf, 256)) {
                  if ((0!=strncmp("performance", buf, 11)) && 
                      (0!=strncmp("powersafe", buf, 9))) {
                          // So it's neither the "performance" nor the "powersafe" governor
                          (void) snprintf(filename, 256, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", cpu);
                          if (read_int(filename, &min)) {
                                  (void) snprintf(filename, 256, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", cpu);
                                  if (read_int(filename, &max)) {
                                          if (min!=max) {
                                                  // wrong governor AND different frequency limits -> scaling
                                                  return 1;
                                          }
                                  } 
                          }
                  }
          } else {
                  // couldn't open file -> no more cores
                  done = 1;
          }
          cpu++;
  }
  
  // couldn't find anything that points to scaling
  return 0;
}


static gid_t get_group_by_name(const char* name) {
  struct group* grp;
	gid_t res = 0;

  while ((0==res) && (NULL != (grp = getgrent()))) {
    if (0==strcmp(name, grp->gr_name)) {
      res = grp->gr_gid;
    }
  }

	endgrent();

  return res;
}

/**
 * Tests wether the owner of this process is in the group 'name'.
 *
 * @returns 0 if the owner of this process is not in the group, non-0 otherwise
 **/
int system_user_in_group(const char *name) {
  gid_t* list = (gid_t*) malloc(MAX_GROUPS * sizeof(gid_t));
  int num_groups, i=0, found=0;
	unsigned int gid;

  if (NULL==list) {
    perror("Cannot allocate group list structure");
    exit(EXIT_FAILURE);
  }

  gid = get_group_by_name(name);
  if (0==gid) {
    fprintf(stderr, "No %s group found\n", name);
    return 0;
  }
  
  num_groups = getgroups(MAX_GROUPS, list);
  
  while (i<num_groups) {
    if (list[i]==gid) {
      found = 1;
      i = num_groups;
    }
    
    i++;
  }
  
  free(list);

  return found;
}


/***
 * Checks for a definition in /etc/security/limits.conf that looks
 * as if it allows RT scheduling priority.
 *
 * @returns 1 if there appears to be such a line
 **/
int system_has_rtprio_limits_conf ()
{
	const char* limits = "/etc/security/limits.conf";
	char cmd[100];

	snprintf (cmd, sizeof (cmd), "grep -q 'rtprio *[0-9][0-9]*' %s", limits);
	if (system (cmd) == 0) {
		return 1;
	}
	return 0;
}


/**
 * Checks for the existence of the 'audio' group on this system
 *
 * @returns 0 if there is no 'audio' group, the group id otherwise
 **/
int system_has_audiogroup() {
	return get_group_by_name("audio") || get_group_by_name ("jackuser");
}


/**
 * Checks for the existence of 'groupname' on this system
 *
 * @returns 0 if there is no group, the group id otherwise
 **/
int system_has_group(const char * name) {
	return get_group_by_name(name);
}


/**
 * Tests wether the owner of this process is in the 'audio' group.
 *
 * @returns 0 if the owner of this process is not in the audio group, non-0 otherwise
 **/
int system_user_in_audiogroup() {
	return system_user_in_group("audio") || system_user_in_group("jackuser");
}


/**
 * Determines wether the owner of this process can enable rt priority.
 *
 * @returns 0 if this process can not be switched to rt prio, non-0 otherwise
 **/
int system_user_can_rtprio() {
  int min_prio;
  struct sched_param schparam;

  memset(&schparam, 0, sizeof(struct sched_param));

  if (-1 == (min_prio = sched_get_priority_min(SCHED_FIFO))) {
    perror("sched_get_priority");
    exit(EXIT_FAILURE);
  }
  schparam.sched_priority = min_prio;  

  if (0 == sched_setscheduler(0, SCHED_FIFO, &schparam)) {
    // TODO: restore previous state
    schparam.sched_priority = 0;
    if (0 != sched_setscheduler(0, SCHED_OTHER, &schparam)) {
      perror("sched_setscheduler");
      exit(EXIT_FAILURE);
    }
    return 1;
  }  

  return 0;
}


long long unsigned int system_memlock_amount() {
	struct rlimit limits;

	if (-1==getrlimit(RLIMIT_MEMLOCK, &limits)) {
		perror("getrlimit on RLIMIT_MEMLOCK");
		exit(EXIT_FAILURE);
	}

	return limits.rlim_max;
}


/**
 * Checks wether the memlock limit is unlimited
 *
 * @returns - 0 if the memlock limit is limited, non-0 otherwise
 **/
int system_memlock_is_unlimited() {
	return ((RLIM_INFINITY==system_memlock_amount())?1:0);
}


long long unsigned int system_available_physical_mem() {
	char buf[256];
	long long unsigned int res = 0;

	if (0<read_string(const_cast<char*>("/proc/meminfo"), buf, sizeof (buf))) {
		if (strncmp (buf, "MemTotal:", 9) == 0) {
			if (sscanf (buf, "%*s %llu", &res) != 1) {
				perror ("parse error in /proc/meminfo");
			} 
		}
	} else {
		perror("read from /proc/meminfo");
	}

	return res*1024;
}


/**
 * Gets the version of the currently running kernel. The string
 * returned has to be freed by the caller.
 *
 * @returns String with the full version of the kernel
 **/
char* system_kernel_version() {
  return NULL;
}



char* system_get_username() {
  char* res = NULL;
  char* name = NULL;

  if ((name = getlogin())) {
    res = strdup(name);
  }

  return res;
}
