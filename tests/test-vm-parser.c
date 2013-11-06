/* Compile with
 * gcc -Wall -g3 -ggdb3 -O0  $(pkg-config --cflags --libs govirt-1.0) ./test-vm-parser.c
 */
#include <govirt/govirt.h>
#include <rest/rest-xml-parser.h>

OvirtCollection *ovirt_collection_new_from_xml(RestXmlNode *root_node,
                                               GType collection_type,
                                               const char *collection_name,
                                               GType resource_type,
                                               const char *resource_name,
                                               GError **error);
int main(int argc, char **argv)
{
    OvirtCollection *collection;
    OvirtResource *vm;
    RestXmlParser *parser;
    RestXmlNode *root_node;
    gchar *xml;
    gsize xml_len;

    if (argc != 2) {
        g_print("Usage: %s filename\n", argv[0]);
        return 1;
    }
    if (!g_file_get_contents(argv[1], &xml, &xml_len, NULL)) {
        g_warning("failed to load %s", argv[1]);
        return 2;
    }
    parser = rest_xml_parser_new();
    root_node = rest_xml_parser_parse_from_data(parser, xml, xml_len);
    if (root_node == NULL) {
        g_warning("failed to parse %s", argv[1]);
        return 3;
    }
    collection = ovirt_collection_new_from_xml(root_node,
                                               OVIRT_TYPE_COLLECTION, "vms",
                                               OVIRT_TYPE_VM, "vm",
                                               NULL);
    rest_xml_node_unref(root_node);
    g_object_unref(parser);
    if (collection == NULL) {
        g_warning("failed to parse collection");
        return 4;
    }

    vm = ovirt_collection_lookup_resource(collection, "win");
    if (vm == NULL) {
        g_warning("could not find 'win'");
        return 5;
    } else {
        char *guid;
        g_object_get(G_OBJECT(vm), "guid", &guid, NULL);
        g_print("win: %s\n", guid);
        g_free(guid);
    }

    vm = ovirt_collection_lookup_resource(collection, "winbak");
    if (vm == NULL) {
        g_warning("could not find 'winbak'");
        return 6;
    } else {
        char *guid;
        g_object_get(G_OBJECT(vm), "guid", &guid, NULL);
        g_print("winbak: %s\n", guid);
        g_free(guid);
    }
    g_object_unref(collection);

    return 0;
}
