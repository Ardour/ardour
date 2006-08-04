#include <pbd/command.h>

class XMLNode;

XMLNode &Command::serialize()
{
    XMLNode *node = new XMLNode ("Command");
    // TODO
    return *node;
}
