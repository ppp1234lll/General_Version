#ifndef __SNMP_OID_H__
#define __SNMP_OID_H__

typedef enum {
    SNMP_TYPE_INTEGER,
    SNMP_TYPE_OCTETSTRING,
    SNMP_TYPE_COUNTER,
    SNMP_TYPE_GAUGE
} snmp_data_type_t;

typedef struct {
    const char *name;      // Item name
    const char *oid;       // OID string
    snmp_data_type_t type; // Data type
    const char *note;      // Note
    int writable;          // Writable (1=write, 0=read only)
} snmp_oid_info_t;

static const snmp_oid_info_t snmp_oid_table[] = {
    {"Device MAC", ".1.3.6.1.4.1.192.10.160.4.2.0", SNMP_TYPE_OCTETSTRING, "", 0},
    {"Device Model", ".1.3.6.1.4.1.192.10.160.4.1.0", SNMP_TYPE_OCTETSTRING, "", 0},
    {"Serial Number", ".1.3.6.1.4.1.192.10.160.4.3.0", SNMP_TYPE_OCTETSTRING, "", 0},
    {"Port Admin Status", ".1.3.6.1.2.1.2.2.1.7.xx", SNMP_TYPE_INTEGER, "Set, controllable port switch, 1=up, 2=down, default all ports up", 1},
    {"Port Oper Status", ".1.3.6.1.2.1.2.2.1.8.xx", SNMP_TYPE_INTEGER, "Get, read current port status, 1=up, 2=down", 0},
    {"Port Out Octets", ".1.3.6.1.2.1.2.2.1.16.xx", SNMP_TYPE_COUNTER, "ifOutOctets, output bytes, xx=port number", 0},
    {"Port Out Discards", ".1.3.6.1.2.1.2.2.1.19.xx", SNMP_TYPE_COUNTER, "ifOutDiscards, output discards, xx=port number", 0},
    {"Port In Octets", ".1.3.6.1.2.1.2.2.1.10.xx", SNMP_TYPE_COUNTER, "ifInOctets, input bytes, xx=port number", 0},
    {"Port In Discards", ".1.3.6.1.2.1.2.2.1.13.xx", SNMP_TYPE_COUNTER, "ifInDiscards, input discards, xx=port number", 0},
    {"LLDP Neighbor ChassisIdSubtype", ".1.0.8802.1.1.2.1.4.1.1.4", SNMP_TYPE_INTEGER, "Neighbor chassis ID subtype", 0},
    {"LLDP Neighbor ChassisId", ".1.0.8802.1.1.2.1.4.1.1.5", SNMP_TYPE_OCTETSTRING, "Neighbor chassis ID", 0},
    {"LLDP Neighbor PortIdSubtype", ".1.0.8802.1.1.2.1.4.1.1.6", SNMP_TYPE_INTEGER, "Neighbor port ID subtype", 0},
    {"LLDP Neighbor PortId", ".1.0.8802.1.1.2.1.4.1.1.7", SNMP_TYPE_OCTETSTRING, "Neighbor port ID", 0},
    {"LLDP Neighbor PortDesc", ".1.0.8802.1.1.2.1.4.1.1.8", SNMP_TYPE_OCTETSTRING, "Neighbor port description", 0},
    {"LLDP Neighbor SysName", ".1.0.8802.1.1.2.1.4.1.1.9", SNMP_TYPE_OCTETSTRING, "Neighbor system name", 0},
    {"LLDP Neighbor SysDesc", ".1.0.8802.1.1.2.1.4.1.1.10", SNMP_TYPE_OCTETSTRING, "Neighbor system description", 0},
    {"LLDP Global Switch", ".1.3.6.1.4.1.192.2.3.1.1.0", SNMP_TYPE_INTEGER, "Default is 1 (enabled)", 1},
    {"LLDP Entry Count", ".1.3.6.1.4.1.192.2.3.3.1.1.1.port.index", SNMP_TYPE_INTEGER, "Index, LLDP entry count", 0},
    {"Neighbor MAC", ".1.3.6.1.4.1.192.2.3.3.1.2.1.port.index", SNMP_TYPE_OCTETSTRING, "", 0},
    {"Neighbor Port ID", ".1.3.6.1.4.1.192.2.3.3.1.3.1.port.index", SNMP_TYPE_OCTETSTRING, "", 0},
    {"Neighbor Alive Time", ".1.3.6.1.4.1.192.2.3.3.1.4.1.port.index", SNMP_TYPE_OCTETSTRING, "", 0},
    {"Neighbor Port Description", ".1.3.6.1.4.1.192.2.3.3.1.5.1.port.index", SNMP_TYPE_OCTETSTRING, "", 0},
    {"Neighbor System Name", ".1.3.6.1.4.1.192.2.3.3.1.6.1.port.index", SNMP_TYPE_OCTETSTRING, "", 0},
    {"Neighbor System Description", ".1.3.6.1.4.1.192.2.3.3.1.7.1.port.index", SNMP_TYPE_OCTETSTRING, "", 0},
    {"Neighbor System Capability", ".1.3.6.1.4.1.192.2.3.3.1.8.1.port.index", SNMP_TYPE_OCTETSTRING, "", 0},
    {"Neighbor Management IP", ".1.3.6.1.4.1.192.2.3.3.1.9.1.port.index", SNMP_TYPE_OCTETSTRING, "", 0},
    {"Local Port", ".1.3.6.1.4.1.192.2.3.3.1.10.1.port.index", SNMP_TYPE_OCTETSTRING, "", 0},
    {"VLAN ID", ".1.3.6.1.4.1.192.2.3.3.1.11.1.port.index", SNMP_TYPE_INTEGER, "", 0},
    {"Port Description", ".1.3.6.1.2.1.2.2.1.2.xx", SNMP_TYPE_OCTETSTRING, "ifDescr, port description, xx=port number", 0},
    {"Port Negotiation Speed", ".1.3.6.1.2.1.2.2.1.5.xx", SNMP_TYPE_GAUGE, "ifSpeed, xx=port number", 0},
    {"Port Up/Down", ".1.3.6.1.2.1.2.2.1.8.xx", SNMP_TYPE_INTEGER, "IfOperStatus, current port status, xx=port number", 0},
    {"MAC Table Entry Count", ".1.3.6.1.4.1.192.2.14.1.1.0", SNMP_TYPE_INTEGER, "", 0},
    {"MAC Address", ".1.3.6.1.4.1.192.2.14.2.1.0", SNMP_TYPE_OCTETSTRING, "", 0},
    {"MAC Address Port", ".1.3.6.1.4.1.192.2.14.2.2.0", SNMP_TYPE_INTEGER, "", 0},
    {"VLAN Mode", ".1.3.6.1.4.1.192.2.2.1.1.3.xx", SNMP_TYPE_INTEGER, "1=access, 2=trunk, 3=hybrid", 1},
    {"Pvid", ".1.3.6.1.4.1.192.2.2.1.1.4.xx", SNMP_TYPE_INTEGER, "Port Pvid", 1},
    {"Untag VLAN", ".1.3.6.1.4.1.192.2.2.1.1.5.xx", SNMP_TYPE_OCTETSTRING, "Untag VLAN", 0},
    {"Tag VLAN", ".1.3.6.1.4.1.192.2.2.1.1.6.xx", SNMP_TYPE_OCTETSTRING, "Tag VLAN", 0},
    {"PoE Port Power Switch", ".1.3.6.1.4.1.192.2.32.1.1.9.xx", SNMP_TYPE_INTEGER, "set&get, controllable PoE port power switch, default all ports enabled", 1},
    {"Switch Uptime", ".1.3.6.1.2.1.1.3.0", SNMP_TYPE_COUNTER, "", 0}
    // Switch temperature has no OID
};

#endif // __SNMP_OID_H__
