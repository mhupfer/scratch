/******************************************************************************
 * Copyright (c) 2024 BlackBerry Limited. All rights reserved.
 *
 * BlackBerry Limited and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation. Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from BlackBerry Limited
 * is strictly prohibited.
 ********************************************************************************
 @file       bk1_posix_openpt().c
 ********************************************************************************

 @brief     blackbox test for posix_openpt()

 @date      2024-02-13

 @author    Ahnaful Hoque (ahoque)

 @env

 @keywords  posix_openpt()

 @note

 *******************************************************************************/

/*--------------------------------------------------------------------------*
 *                                   INCLUDE                                *
 *--------------------------------------------------------------------------*/
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>

#include <regress/regress.h>
#include <regress/testpoint.h>
#include <sys/resource.h>

/*--------------------------------------------------------------------------*
 *                                    MACROS                                *
 *--------------------------------------------------------------------------*/
#define INVALID_FLAG 0x80000000 // used in Test case 5

/*--------------------------------------------------------------------------*
 *                                   ANALYSIS                                *
 *---------------------------------------------------------------------------*
 *
 * step 1: Requirements development
 *   - This function will Open a pseudo-terminal device.
 *   - flags can be provided which specify the file status flags and file access
 *    modes of the open file description.
 *
 * step 2: State components formalization
 *   Param oflag: specify the file status flags
 *   Return value: file descriptor for the master pseudo-terminal, or -1 if an
 *   error occurred (errno is set)
 *   Errno: EMFILE, ENFILE, EINVAL, EAGAIN
 *
 * step 3: Equivalence class analysis
 *   flags:
 *   a combination of the following:
 *       O_RDWR
 *        O_NOCTTY
 *   Return value:
 *       OK: file descriptor for the master pseudo-terminal,
 *       ERR: -1 is returned, errno is set
 *   Errno
 *       ENFILE: Can be achieved by exhausting all allowable number of files in
 *        the system
 *       EMFILE: Can be achieved by exhausting all file descriptors available to
 *        the process
 *       EINVAL: Can be achieved by passing an invalid flag
 *       EAGAIN: Can be achieved by repeatedly calling posix_openpt() to exhaust
 *        all available pseudo-terminal resources
 *       NOTE: ENFILE cannot be tested for.
 *
 * So my test cases would be:
 *
 *   (O_RDWR)    -> Function worked as expected. Return a file descriptor value
 *    for the master pseudo-terminal
 *   (O_NOCTTY)  -> Function worked as expected. Return a file descriptor value
 *    for the master pseudo-terminal
 *   (O_RDWR | O_NOCTTY) -> Function worked as expected. Return a file
 *descriptor value for the master pseudo-terminal (O_RDWR) -> set low rlimit
 *number. I set it to 3.
 *             -> return -1. Upon calling posix_openpt(), this will try to open
 *    the fourth pseudo-terminal device but the limit is 3
 *             -> errno should be set to EMFILE. But the errno is set to EAGAIN.
 *    Bug in the posix_openpt() source code
 *   (INVALID_FLAG) -> return -1. Errno is set to EINVAL
 *   (O_NOCTTY) -> return -1. Errno is set to EAGAIN. Repeatedly call
 *    posix_openpt() to exhaust all pseudo-terminal resources
 *--------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
    char slave_name[64];
    int master_fd;
    char read_buf[80];
    int slavefd;
    char buf[1024] = "hello";
    ssize_t bytes_written;
    int size_read;
    teststart(argv[0]);

    /****************************************************************************
     *
     * Test Case: Test for ENFILE error
     *
     *****************************************************************************
     *
     *    NOTE: posix_openpt() function returns ENFILE error when the maximum
     *          allowable number of files is currently open in the system.
     *          However, this number is very large, and there is no practical way
     *to directly test for this error. Therefore, there are no specific test cases
     *          checking for this error in this file.
     *
     ****************************************************************************/

    /****************************************************************************
     *
     * Test Case: Pass valid flag with O_RDWR
     *
     *****************************************************************************
     *
     *    Purpose:
     *        This test case will check if posix_openpt() work as expected
     *        if correct flag (O_RDWR) is passed as parameter
     *  IMPORTANT: posix_openpt() contains a bug and it does not
     *             change the controlling terminal when O_NOCTTY is not passed
     *             as parameter
     *
     * https://jira.bbqnx.net/browse/COREOS-125663
     *
     ****************************************************************************/

    testpntbeginf("Test case: Test for success case with O_RDWR flag");
    do
    {
        master_fd = posix_openpt(O_RDWR);
        if (master_fd == -1)
        {
            testpntfailf("Error opening pseudoterminal master: %d: %s", errno,
                         strerror(errno));
            break;
        }
        if (grantpt(master_fd) == -1)
        {
            testpntunresf("Error setting up pseudoterminal slave grantpt(): %d: %s",
                          errno, strerror(errno));
            break;
        }
        if (unlockpt(master_fd) == -1)
        {
            testpntunresf("unlockpt: %d: %s", errno, strerror(errno));
            break;
        }
        if (ptsname_r(master_fd, slave_name, sizeof(slave_name)) != 0)
        {
            testpntunresf("ptsname_r: %d: %s", errno, strerror(errno));
            break;
        }

        slavefd = open(slave_name, O_RDWR);
        if (slavefd < 0)
        {
            testerror("Error in slavefd");
            break;
        }

        bytes_written = write(slavefd, buf, sizeof(buf));
        if (bytes_written == -1)
        {
            testpntfailf("Error writing to slavefd %d:%s", errno, strerror(errno));
            break;
        }

        size_read = read(master_fd, &read_buf, sizeof(read_buf));
        if (read(master_fd, &read_buf, sizeof(read_buf)) == -1)
        {
            testpntfailf("Error reading from the file descriptor %d:%s", errno,
                         strerror(errno));
            break;
        }
        if (strcmp(slave_name, ctermid(NULL)) != 0)
        {
            testpntfailf("Pty is not the control process");
            testnotef("current terminal:%s ", ctermid(NULL));
            testnotef("slave pathname:%s", slave_name);
        }
        else
        {
            testpntpassf("Test case O_RDWR flag Success (fd: %d), (slave_pathname: "
                         "%s), (bytes_written: %ld), (slave_fd:%d), (size_read: %d)",
                         master_fd, slave_name, bytes_written, slavefd, size_read);
        }

        if (close(slavefd) == -1)
        {
            testerror("Error closing slavefd");
        }

        if ((master_fd != -1) && (close(master_fd) != 0))
        {
            testerrorf("close() failed on master: %d:(%s)", errno, strerror(errno));
        }
    } while (0);

    testpntend();

    /****************************************************************************
     *
     * Test Case: Pass valid flag with O_NOCTTY
     *
     *****************************************************************************
     *
     *    Purpose:
     *        This test case will check if posix_openpt() work as expected
     *        if correct flag (O_NOCTTY) is passed as parameter
     *
     ****************************************************************************/

    testpntbeginf("Test case: Test for success case with O_NOCTTY flag");
    do
    {
        master_fd = posix_openpt(O_NOCTTY);
        if (master_fd == -1)
        {
            testpntfailf("Error opening pseudoterminal master: %d: %s %d", errno,
                         strerror(errno), master_fd);
            break;
        }
        else if (grantpt(master_fd) == -1)
        {
            testpntunresf("Error setting up pseudoterminal slave grantpt(): %d: %s",
                          errno, strerror(errno));
            break;
        }
        else if (ptsname_r(master_fd, slave_name, sizeof(slave_name)) != 0)
        {
            testpntunresf("ptsname_r: %d: %s", errno, strerror(errno));
            break;
        }

        if (strcmp(slave_name, ctermid(NULL)) == 0)
        {
            testpntfailf("Pty is the control process");
            testnotef("current terminal:%s ", ctermid(NULL));
            testnotef("slave pathname:%s", slave_name);
        }
        else
        {
            testpntpassf(
                "Test case O_NOCTTY flag Success (fd: %d), (slave_pathname: %s)",
                master_fd, slave_name);
        }

        if ((master_fd != -1) && (close(master_fd) != 0))
        {
            testerrorf("close() failed on master: %d:(%s)", errno, strerror(errno));
        }
    } while (0);
    testpntend();

    /****************************************************************************
     *
     * Test Case: Pass valid flag with O_RDWR | O_NOCTTY
     *flag
     *
     *****************************************************************************
     *
     *    Purpose:
     *        This test case will check if posix_openpt() work as expected
     *        if correct flag (O_RDWR | O_NOCTTY flag) is passed as parameter
     *
     ****************************************************************************/
    testpntbeginf("Test case: Test for success case with O_RDWR | O_NOCTTY flag");
    do
    {
        master_fd = posix_openpt(O_RDWR | O_NOCTTY);
        if (master_fd == -1)
        {
            testpntfailf("Error opening a terminal %d: %s", errno, strerror(errno));
            break;
        }
        if (grantpt(master_fd) == -1)
        {
            testpntunresf("Error setting up pseudoterminal slave grantpt(): %d: %s",
                          errno, strerror(errno));
            break;
        }
        if (unlockpt(master_fd) == -1)
        {
            testpntunresf("unlockpt: %d: %s", errno, strerror(errno));
            break;
        }
        if (ptsname_r(master_fd, slave_name, sizeof(slave_name)) != 0)
        {
            testpntunresf("ptsname_r: %d: %s", errno, strerror(errno));
            break;
        }

        slavefd = open(slave_name, O_RDWR);
        if (slavefd < 0)
        {
            testerrorf("Error in slavefd %d:%s", errno, strerror(errno));
            break;
        }

        bytes_written = write(slavefd, buf, sizeof(buf));
        if (bytes_written == -1)
        {
            testpntfailf("Error writing to slavefd %d:%s", errno, strerror(errno));
            break;
        }

        size_read = read(master_fd, &read_buf, sizeof(read_buf));
        if (read(master_fd, &read_buf, sizeof(read_buf)) == -1)
        {
            testpntfailf("Error reading from the file descriptor %d:%s", errno,
                         strerror(errno));
            break;
        }
        if (strcmp(slave_name, ctermid(NULL)) == 0)
        {
            testpntfailf("Pty is the control process");
            testnotef("current terminal:%s ", ctermid(NULL));
            testnotef("slave pathname:%s", slave_name);
        }
        else
        {
            testpntpassf("Test case O_RDWR | O_NOCTTY flag Success (fd: %d), "
                         "(slave_pathname: %s)",
                         master_fd, slave_name);
        }

        if ((master_fd != -1) && (close(master_fd) != 0))
        {
            testerrorf("close() failed on master: %d:(%s)", errno, strerror(errno));
        }
    } while (0);
    testpntend();

    /****************************************************************************
     *
     * Test Case: Test for EMFILE error
     *
     *****************************************************************************
     *
     *    Purpose:
     *        This test case will check if posix_openpt() return EMFILE error when
     *        all file descriptor available to the process is open
     *
     *  IMPORTANT:
     *      posix_openpt() source code contains a bug. It does not return EMFILE
     *      error when expected. The posix_openpt() function calls open(), and if
     *      open() fails with EMFILE error, posix_openpt() should return EMFILE.
     *      However, in the source code posix_openpt only retuns EINVAL or EAGAIN
     *errors and does not return EMFILE or ENFILE errors.
     *
     * Link to jira ticket:
     * https://jira.bbqnx.net/browse/COREOS-125485
     ****************************************************************************/
    testpntbeginf("Test case: Test for EMFILE error");
    do
    {
        struct rlimit rl;
        rl.rlim_cur = 3;
        rl.rlim_max = 3;

        if (setrlimit(RLIMIT_NOFILE, &rl) != 0)
        {
            testpntunresf("Error in setrlimit %d: %s", errno, strerror(errno));
            break;
        }

        master_fd = posix_openpt(O_RDWR);
        if (master_fd == -1)
        {
            if (errno == EMFILE)
            {
                testpntpassf("EMFILE error received as expected %d: %s", errno,
                             strerror(errno));
            }
            else
            {
                testpntfailf("Different Error received %d: %s", errno, strerror(errno));
            }
        }
        else
        {
            testpntfailf("posix_openpt worked unexpectedly. File Descriptor: %d",
                         master_fd);
        }

        if ((master_fd != -1) && (close(master_fd) != 0))
        {
            testerrorf("close() failed on master: %d:(%s)", errno, strerror(errno));
        }
    } while (0);
    testpntend();

    /****************************************************************************
     *
     * Test Case: Test for EINVAL error
     *
     *****************************************************************************
     *
     *    Purpose:
     *        This test case will check if posix_openpt() returns EINVAL upon
     *        passing an invalid flag
     *
     ****************************************************************************/
    testpntbeginf("Test case: Test for EINVAL error");
    master_fd = posix_openpt(INVALID_FLAG);

    if (master_fd == -1)
    {
        if (errno == EINVAL)
        {
            testpntpassf("Invalid flag detected as expected %d: %s", errno,
                         strerror(errno));
        }
        else
        {
            testpntfailf("Test failed: Expected EINVAL, but got another error %d: %s",
                         errno, strerror(errno));
        }
    }
    else
    {
        testpntfailf("posix_openpt() worked unexpectedly %d", master_fd);
    }

    if ((master_fd != -1) && (close(master_fd) != 0))
    {
        testerrorf("close() failed on master: %d:(%s)", errno, strerror(errno));
    }
    testpntend();

    /****************************************************************************
     *
     * Test Case: Test for EAGAIN error
     *
     *****************************************************************************
     *
     *    Purpose:
     *        This test case will check if posix_openpt() returns EAGAIN when
     *        all file descriptors are open.
     *
     ****************************************************************************/
    testpntbeginf("Test case: Test for EAGAIN error");
    while ((master_fd = posix_openpt(O_NOCTTY)) != -1);
    if (master_fd == -1)
    {
        if (errno == EAGAIN)
        {
            testpntpassf("All file descriptors are open as expected %d: %s", errno,
                         strerror(errno));
        }
        else
        {
            testpntfailf("Expected EAGAIN, but received %d: %s", errno,
                         strerror(errno));
        }
    }
    else
    {
        testpntfailf("posix_openpt() worked unexpectedly %d", master_fd);
    }

    if ((master_fd != -1) && (close(master_fd) != 0))
    {
        testerrorf("close() failed on master: %d:(%s)", errno, strerror(errno));
    }
    testpntend();

    teststop(argv[0]);
    return teststatus();
}
