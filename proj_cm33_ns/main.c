/*******************************************************************************
* File Name        : main.c
*
* Description      : This source file contains the main routine for non-secure
*                    application running on CM33 CPU.
*
* Related Document : See README.md
*
********************************************************************************
* Copyright 2023-2025, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "cybsp.h"
#include "retarget_io_init.h"
#include <inttypes.h>
#include <string.h>


/*******************************************************************************
* Macros
*******************************************************************************/

/* Set "WRITE_TO_RRAM_GENERAL_OTP_REGION" as 1 to write to OTP */
#define WRITE_TO_RRAM_GENERAL_OTP_REGION    (0U)
#define OTP_ADDRESS                         (CY_RRAM_GENERAL_OTP_MMIO_NS_START_ADDRESS)
#define OTP_PAGE_SIZE_BYTES                 (CY_RRAM_BLOCK_SIZE_BYTES)
#define RRAM_ADDRESS                        (CYMEM_CM33_0_user_nvm_START)
#define RRAM_PAGE_SIZE_BYTES                (CY_RRAM_BLOCK_SIZE_BYTES)

/* Test data to write into OTP */
#define OTP_WRITE_DATA                      (0x1FU)
#define OTP_WRITE_DATA_SIZE                 (1U)

/* Number of bytes to print in a line */
#define NUM_BYTES_PER_LINE                  (16U)

/* The timeout value in microseconds used to wait for CM55 core to be booted */
#define CM55_BOOT_WAIT_TIME_USEC            (10U)

/* Print delay */
#define PRINT_DELAY_MS                      (50U)

/* LED blink delay */
#define LED_TOGGLE_DELAY_MS                 (1000U)

#define SUCCESS_STATUS                      (0U)


/* App boot address for CM55 project */
#define CM55_APP_BOOT_ADDR          (CYMEM_CM33_0_m55_nvm_START + \
                                     CYBSP_MCUBOOT_HEADER_SIZE)

/*******************************************************************************
* Function Name: check_status
********************************************************************************
* Summary:
*  Error handling routine which indicates operation error by switching on the
*  User LED1 and printing the error message.
*
* Parameters:
*  message - Error message to print if error found.
*  status  - Error status code.
*
* Return:
*  void
*
*******************************************************************************/
void check_status(char *message, uint32_t status)
{
    if (SUCCESS_STATUS != status)
    {
        printf("\r\n=====================================================\r\n");
        printf("\nFAIL: %s\r\n", message);
        printf("Error Code: 0x%08"PRIX32"\n", status);
        printf("\r\n=====================================================\r\n");

        /* On failure, turn the User LED1 ON */
        Cy_GPIO_Set(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN);

        /* Wait forever here when error occurs. */
        while(true);
    }
}


/*******************************************************************************
* Function Name: print_array
********************************************************************************
* Summary:
*  Prints the content of the buffer to the UART console.
*
* Parameters:
*  message - message to print before array output
*  buf - buffer to print on the console.
*  size - size of the buffer.
*
* Return:
*  void
*
*******************************************************************************/
void print_array(char *message, uint8_t *buf, uint32_t size)
{
    printf("\r\n%s (%"PRIu32" bytes):\r\n", message, size);
    printf("-------------------------\r\n");

    for (uint32_t index = 0; index < size; index++)
    {
        printf("0x%02X ", buf[index]);

        if (!((index + 1) % NUM_BYTES_PER_LINE))
        {
            printf("\r\n");
        }
    }
}


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function of the CM33 non-secure application.
*
* It initializes the device and board peripherals. It also initializes the
* retarget-io middleware to be used with the debug UART port to print
* operation status as well as any encountered error messages.
* Post initialization it carries out NVM read-write operations.
* Once the operation is successful, it enables CM55 CPU and toggles user LED1
* at a frequency of 1 Hz.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    uint32_t  index                 = 0;
    cy_rslt_t result                = CY_RSLT_SUCCESS;
    cy_en_rram_status_t rram_status = CY_RRAM_SUCCESS;

    uint8_t write_buff[RRAM_PAGE_SIZE_BYTES] = { 0xFF };
    uint8_t read_buff[RRAM_PAGE_SIZE_BYTES]  = { 0xFF };
#if (WRITE_TO_RRAM_GENERAL_OTP_REGION)
    uint8_t otp_data = OTP_WRITE_DATA;
#endif

    /* Initialize the device and board peripherals. */
    result = cybsp_init();

    /* Board initialization failed. Stop program execution. */
    if (CY_RSLT_SUCCESS != result)
    {
        handle_app_error();
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io middleware */
    init_retarget_io();

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen. */
    printf("\x1b[2J\x1b[;H");

    printf("****************** "
            "PSOC Edge MCU: Basic read and write on NVM "
            "****************** \r\n\n");

    printf("\r\n1.Reading the data from address before write\r\n");

    rram_status = Cy_RRAM_TSReadByteArray(RRAMC0, RRAM_ADDRESS, read_buff,
                                          RRAM_PAGE_SIZE_BYTES);
    check_status("NVM read failure", rram_status);

    print_array("Read Data", read_buff, RRAM_PAGE_SIZE_BYTES);

    /* Prepare the write buffer data */
    for(index = 0; index < RRAM_PAGE_SIZE_BYTES; index++)
    {
        write_buff[index] = (uint8_t)index;
    }

    /* Write the content of the write buffer to the memory */
    printf("\r\n2. Writing data to memory\r\n");

    rram_status = Cy_RRAM_NvmWriteBlock(RRAMC0, RRAM_ADDRESS, write_buff);
    check_status("NVM write failure", rram_status);

    print_array("Written Data", write_buff, RRAM_PAGE_SIZE_BYTES);

    /* Read back after Write for verification */
    printf("\r\n3. Reading back for verification\r\n");
    rram_status = Cy_RRAM_TSReadByteArray(RRAMC0, RRAM_ADDRESS, read_buff,
                                          RRAM_PAGE_SIZE_BYTES);
    check_status("NVM read failure", rram_status);

    print_array("Read Data", read_buff, RRAM_PAGE_SIZE_BYTES);

    /* Check if the transmitted and received arrays are equal */
    check_status("Read data does not match with written data. Read/Write "
                 "operation failure.", memcmp(write_buff, read_buff,
                 RRAM_PAGE_SIZE_BYTES));

    printf("\r\n=========================================================\r\n");
    printf("\r\nSUCCESS: Read data matches with written data!\r\n");
    printf("\r\n=========================================================\r\n");

#if (WRITE_TO_RRAM_GENERAL_OTP_REGION)
    printf("Press USER BTN1 to write to the OTP region\r\n");

    printf("Wait for User button press\r\n");
    while(CYBSP_BTN_OFF ==  Cy_GPIO_Read(CYBSP_USER_BTN1_PORT,
                                         CYBSP_USER_BTN1_PIN));

    printf("\r\nButton Pressed\n");
    printf("\r\n1.Writing the data to OTP address 0x%lx\r\n", OTP_ADDRESS);

    rram_status = Cy_RRAM_OtpWriteByteArray(RRAMC0, OTP_ADDRESS, &otp_data,
                                            OTP_WRITE_DATA_SIZE);

    check_status("OTP region write failure", rram_status);
#else
    printf("\r\n1.Write Skipped for OTP address 0x%lx\r\n", OTP_ADDRESS);
#endif /* WRITE_TO_RRAM_GENERAL_OTP_REGION */

    printf("\r\n2.Reading the data from OTP address 0x%lx\r\n", OTP_ADDRESS);

    rram_status = Cy_RRAM_TSReadByteArray(RRAMC0, OTP_ADDRESS, read_buff,
                                          OTP_PAGE_SIZE_BYTES);
    check_status("OTP region read failure", rram_status);

    print_array("Read Data", read_buff, OTP_PAGE_SIZE_BYTES);

    /* Enable CM55. */
    /* CM55_APP_BOOT_ADDR must be updated if CM55 memory layout is changed.*/
    Cy_SysEnableCM55(MXCM55, CM55_APP_BOOT_ADDR, CM55_BOOT_WAIT_TIME_USEC);

    for (;;)
    {
        Cy_GPIO_Inv(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN);
        Cy_SysLib_Delay(LED_TOGGLE_DELAY_MS);
    }
}


/* [] END OF FILE */
