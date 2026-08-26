module Engine.Core.Command;

import std;

namespace Vyroda
{
  //@todo WIP
  Response Command::execRequest()
  {
    Response response;

    _request();

    return response;
  }

  WhenToExecute Command::getWhenToExecute() const
  {
    return _when;
  }
}
