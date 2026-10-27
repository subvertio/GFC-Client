"""
Runs a GFC backend as an isolated local test node, for client development.

The backend's P2P layer (LAN broadcast, the bootstrap peer and the tracker
relay) would otherwise copy test accounts, wallets and seeds onto the public
network. This wrapper switches it off without modifying the backend.

Use Scripts/RunLocalNode.ps1 rather than calling this directly. It sets
GFC_BACKEND_DIR (a GFCCoin checkout) and GFC_DATA_DIR (a scratch folder), then
runs: python -m uvicorn local_node:app --host 127.0.0.1 --port 8000
"""
import os
import sys

backend = os.environ.get("GFC_BACKEND_DIR")
if not backend or not os.path.isfile(os.path.join(backend, "main.py")):
    sys.exit("Set GFC_BACKEND_DIR to a GFCCoin checkout (the folder with main.py).")
if not os.environ.get("GFC_DATA_DIR"):
    sys.exit("Set GFC_DATA_DIR to a scratch folder so the test node never touches real data.")

os.environ["GFC_BOOTSTRAP_PEERS"] = ""  # no bootstrap peer, even if P2P were started
sys.path.insert(0, backend)

import main  # noqa: E402  (the backend's FastAPI app)


async def _p2p_disabled():
    print("[local test node] P2P sync disabled: nothing leaves this machine")


# startup_event() looks these names up in main's namespace when it runs.
main.start_p2p = _p2p_disabled
main.stop_p2p = _p2p_disabled
main._ensure_firewall_rules = lambda: None

app = main.app
