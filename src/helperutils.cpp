#include <QString>
#include <pwd.h>
#include "helperutils.h"

QString getUserFromUid(uid_t uid)
{
  struct passwd *pw = getpwuid(uid);
  return pw ? QString(pw->pw_name) : QString("unknown");
}