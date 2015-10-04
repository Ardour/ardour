#ifndef __systemtest_h__
#define __systemtest_h__

/**
 * GPL, yabbadabba
 *
 * Set of functions to gather system information for the jack setup wizard.
 *
 * @author Florian Faber, faber@faberman.de
 *
 * @version 0.1 (2009-01-15) [FF]
 *              - initial version
 *
 **/


/**
 * This function checks for the existence of known frequency scaling mechanisms
 * in this system.
 *
 * @returns 0 if the system has no frequency scaling capabilities non-0 otherwise.
 **/
int system_has_frequencyscaling();


/**
 * This function determines wether the CPU has a variable clock speed if frequency
 * scaling is available.
 *
 * @returns 0 if system doesn't use frequency scaling at the moment, non-0 otherwise
 **/
int system_uses_frequencyscaling();

/**
 * Tests wether the owner of this process is in the group 'name'.
 *
 * @returns 0 if the owner of this process is not in the group, non-0 otherwise
 **/
int system_user_in_group(const char *name);

/***
 * Checks for a definition in /etc/security/limits.conf that looks
 * as if it allows RT scheduling priority.
 *
 * @returns 1 if there appears to be such a line
 **/
int system_has_rtprio_limits_conf ();

/**
 * Checks for the existence of the 'audio' group on this system
 *
 * @returns 0 is there is no 'audio' group, non-0 otherwise
 **/
int system_has_audiogroup();

/**
 * Checks for the existence of a group named 'name' on this system
 *
 * @returns 0 if not found, non-0 otherwise
 **/
int system_has_group(const char * name);

/**
 * Tests wether the owner of this process is in the 'audio' group.
 *
 * @returns 0 if the owner of this process is not in the audio group, non-0 otherwise
 **/
int system_user_in_audiogroup();


/**
 * Determines wether the owner of this process can enable rt priority.
 *
 * @returns 0 if this process can not be switched to rt prio, non-0 otherwise
 **/
int system_user_can_rtprio();


long long unsigned int system_memlock_amount();


/**
 * Checks wether the memlock limit is unlimited
 *
 * @returns 0 if the memlock limit is limited, non-0 otherwise
 **/
int system_memlock_is_unlimited();


long long unsigned int system_available_physical_mem();


/**
 * Gets the version of the currently running kernel
 *
 * @returns String with the full version of the kernel
 **/
char* system_kernel_version();


/**
 * Returns the username. The caller is in charge of disposal of
 * the returned name.
 *
 * @returns Pointer to a username or NULL
 **/
char* system_get_username();

#endif /* __jack_systemtest_h__ */
