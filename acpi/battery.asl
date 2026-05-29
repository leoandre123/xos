/*
 * Fake ACPI battery for QEMU.
 * Injected at boot via: -acpitable file=acpi/battery.aml
 *
 * Compile: iasl battery.asl   (produces battery.aml)
 * Install: sudo apt install acpica-tools
 *
 * _BST and _BIF return static packages (no EC access needed).
 * All capacities are in mWh, rates in mW, voltage in mV (power_unit=0).
 */
DefinitionBlock ("battery.aml", "SSDT", 2, "XOSBAT", "BATFAKE", 0x00000001)
{
    Scope (\_SB)
    {
        Device (BAT0)
        {
            Name (_HID, "PNP0C0A")
            Name (_UID, Zero)

            Method (_STA, 0, NotSerialized)
            {
                Return (0x1F)   /* present, enabled, functional, showing in UI */
            }

            Method (_BIF, 0, NotSerialized)
            {
                Return (Package (0x0D)
                {
                    0x00000000,     /* Power unit:          0 = mWh/mW */
                    0x00003A98,     /* Design capacity:     15000 mWh  */
                    0x00003A98,     /* Last full charge:    15000 mWh  */
                    0x00000001,     /* Battery technology:  1 = rechargeable */
                    0x00002BEC,     /* Design voltage:      11244 mV   */
                    0x000000C8,     /* Warning capacity:    200 mWh    */
                    0x00000064,     /* Low capacity:        100 mWh    */
                    0x00000001,     /* Capacity granularity 1 */
                    0x00000001,     /* Capacity granularity 2 */
                    "QEMU BAT",     /* Model number  */
                    "0001",         /* Serial number */
                    "LION",         /* Battery type  */
                    "QEMU"          /* OEM info      */
                })
            }

            Method (_BST, 0, NotSerialized)
            {
                Return (Package (0x04)
                {
                    0x00000001,     /* State:     1 = discharging */
                    0x00002710,     /* Rate:      10000 mW (10 W) */
                    0x00001D4C,     /* Remaining: 7500 mWh (~50%) */
                    0x00002BEC      /* Voltage:   11244 mV        */
                })
            }
        }
    }
}
