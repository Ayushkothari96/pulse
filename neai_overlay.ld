/* ====================================================================
 *  NanoEdge AI library RAM section fix
 *  This ensures the .neai and .neai_data sections (used by NanoEdgeAI)
 *  are stored in SRAM, not Flash.
 * ==================================================================== */

SECTION_DATA_PROLOGUE(.neai, (NOLOAD),)
{
    . = ALIGN(4);
    KEEP(*(.neai))
    KEEP(*(.neai_data))
    . = ALIGN(4);
} GROUP_LINK_IN(RAM)
