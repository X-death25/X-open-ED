
#include "OpenEd.h"

// Put function in .data (RAM) instead of the default .text
#define RAM_SECT __attribute__((section(".ramprog")))
// Avoid function to be inlined by compiler
#define NO_INL __attribute__((noinline))

unsigned short ed_cfg;


RAM_SECT NO_INL void OpenEd_DebugLed_ON(void)
{
	 ed_cfg |= CTRL_LED;
	 *((vu16 *) 0xA130E0) = ed_cfg;
}

RAM_SECT NO_INL void OpenEd_DebugLed_OFF(void)
{
	 ed_cfg &= ~CTRL_LED;
	 *((vu16 *) 0xA130E0) = ed_cfg;
}

RAM_SECT NO_INL void OpenEd_Set_Bank0(void)
{
	ed_cfg &= ~CTRL_ROM_BANK;
	*((vu16 *) 0xA130E0) = ed_cfg;
}

RAM_SECT NO_INL void OpenEd_Set_Bank1(void)
{
	//*(vu16*) (CTRL_REG) |=(0x04);
	ed_cfg |= CTRL_ROM_BANK;
	*((vu16 *) 0xA130E0) = ed_cfg;
}

RAM_SECT NO_INL void OpenEd_Start_ROM(void)
{
	ed_cfg &= ~CTRL_ROM_BANK;
	*((vu16 *) 0xA130E0) = ed_cfg;
	asm("move.l 0, %sp");
    asm("move.l 4, %a0");
    asm("jmp (%a0)");
}

/*
 * OpenEd_SPI_Read — identique à spi_r() de Krikzz, légèrement nettoyé
*/

unsigned char OpenEd_SPI_Read(void)
{
    unsigned char ret = 0;

    ed_cfg |= CTRL_SPI_MOSI;
    *((vu16 *) 0xA130E0) = ed_cfg;

    for (unsigned short i = 0; i < 8; i++) {
        *((vu16 *) 0xA130E0) = ed_cfg | CTRL_SPI_MOSI | CTRL_SPI_CLK;
        ret <<= 1;
        ret |= *((vu16 *) 0xA130E0) & CTRL_SPI_MISO; /* bits 1-7 always 0, masque par sécurité */
        *((vu16 *) 0xA130E0) = ed_cfg | CTRL_SPI_MOSI;
    }

    return ret;
}

/*
 * OpenEd_SPI_ReadFast — exploitation de l'auto-CLK hardware documentée par Krikzz :
 * "If SPI_CLK set to 0, then every control register read cycle will generate
 *  automatic positive pulse at SPI_CLK."
 *
 * Chaque lecture du registre pulse CLK automatiquement → pas besoin de toggler.
 * Idéal pour rcvr_mmc() lors du transfert de blocs de données.
 */
 
unsigned char OpenEd_SPI_ReadFast(void)
{
    unsigned char ret = 0;

    /* CLK à 0 pour activer l'auto-pulse, MOSI idle haut */
    ed_cfg &= ~CTRL_SPI_CLK;
    ed_cfg |=  CTRL_SPI_MOSI;
    *((vu16 *) 0xA130E0) = ed_cfg;

    for (unsigned short i = 0; i < 8; i++) {
        ret <<= 1;
        /* La lecture pulse CLK automatiquement et capture MISO */
        ret |= *((vu16 *) 0xA130E0) & CTRL_SPI_MISO;
    }

    return ret;
}

/*
 * OpenEd_SPI_Write — identique à spi_w() de Krikzz
 */
 
void OpenEd_SPI_Write(unsigned char val)
{
    for (unsigned short i = 0; i < 8; i++) {
        *((vu16 *) 0xA130E0) = ed_cfg | (val & CTRL_SPI_MOSI);
        *((vu16 *) 0xA130E0) = ed_cfg | (val & CTRL_SPI_MOSI) | CTRL_SPI_CLK;
        *((vu16 *) 0xA130E0) = ed_cfg | (val & CTRL_SPI_MOSI);
        val <<= 1;
    }
}

unsigned char OpenEd_SPI_Read_Write(unsigned char val)

{
     u8 ret = 0;

    for (unsigned short i = 0; i < 8; i++) {

        *((vu16 *) 0xA130E0) = (val & CTRL_SPI_MOSI) | ed_cfg;
        *((vu16 *) 0xA130E0) = (val & CTRL_SPI_MOSI) | ed_cfg | CTRL_SPI_CLK;
        ret <<= 1;
        ret |= *((vu16 *) 0xA130E0) & CTRL_SPI_MISO;
        *((vu16 *) 0xA130E0) = (val & CTRL_SPI_MOSI) | ed_cfg;
        val <<= 1;
    }

    return ret;
}

void OpenEd_SPI_Select(unsigned char target)
{
    ed_cfg &= ~(CTRL_SDC_SS | CTRL_EXP_SS); /* désélectionne tout */

    if (target == SPI_SEL_SDC) {
        ed_cfg |= CTRL_SDC_SS;
    } else if (target == SPI_SEL_EXP) {
        ed_cfg |= CTRL_EXP_SS;
    }

    *((vu16 *) 0xA130E0) = ed_cfg;
}