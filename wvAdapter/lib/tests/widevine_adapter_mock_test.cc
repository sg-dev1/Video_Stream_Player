#include "adapterInterface.h"

#include <iostream>
#include <string>

int main() {
  AdapterInterface adapter;
  if (!adapter.Init()) {
    std::cerr << "Adapter initialization failed with fake CDM" << std::endl;
    return 1;
  }

  const uint8_t initData[] = {0, 0, 0, 8, 'p', 's', 's', 'h'};
  if (!adapter.CreateSessionAndGenerateRequest(
          initData, sizeof(initData), AdapterInterface::kVideo)) {
    std::cerr << "Asynchronous session creation did not resolve" << std::endl;
    adapter.Close();
    return 2;
  }

  const std::string challenge =
      adapter.GetSessionMessage(AdapterInterface::kVideo);
  if (challenge != "fake-cdm-challenge") {
    std::cerr << "Unexpected session challenge: " << challenge << std::endl;
    adapter.Close();
    return 3;
  }

  const uint8_t license[] = "fake-license";
  if (!adapter.UpdateCurrentSession(
          license, sizeof(license) - 1, AdapterInterface::kVideo)) {
    std::cerr << "Asynchronous license update did not resolve" << std::endl;
    adapter.Close();
    return 4;
  }

  if (adapter.ValidKeyIdsSize(AdapterInterface::kVideo) != 1) {
    std::cerr << "Expected one usable key after license update" << std::endl;
    adapter.Close();
    return 5;
  }

  adapter.Close();
  std::cout << "CDM v11 async integration OK" << std::endl;
  return 0;
}
