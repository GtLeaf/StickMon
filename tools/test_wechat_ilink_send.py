#!/usr/bin/env python3
"""WeChat iLink sendmessage isolation test (host-side, no device needed).

Purpose: reproduce the "sendmessage returns ret=0 but the WeChat client
never shows the message" symptom off-device, and bisect which request
field makes delivery work.

Usage:
  1. Power off the StickMon device (or disable the wechat channel) so it
     stops polling getupdates and does not race this script.
  2. Run:  python3 tools/test_wechat_ilink_send.py --token ilinkbot_xxx
  3. From your phone, send any WeChat message to the bot.
  4. The script captures the inbound context_token, then sends several
     reply variants and prints the server response for each. Watch the
     phone and note which variants actually arrive.

The token is read from the device NVS (claw/wechat_token). Do not commit
real tokens.
"""

import argparse
import base64
import json
import os
import random
import sys
import time
import urllib.request
import urllib.error

BASE = "https://ilinkai.weixin.qq.com"


def post(token, endpoint, body, timeout=40):
    uin = base64.b64encode(str(random.randint(0, 0xFFFFFFFF)).encode()).decode()
    raw = json.dumps(body, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(
        f"{BASE}/ilink/bot/{endpoint}",
        data=raw,
        headers={
            "Content-Type": "application/json",
            "AuthorizationType": "ilink_bot_token",
            "Authorization": f"Bearer {token}",
            "X-WECHAT-UIN": uin,
            "iLink-App-Id": "bot",
            "iLink-App-ClientVersion": "131329",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            text = resp.read().decode("utf-8", "replace")
            return resp.status, text
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", "replace")
    except Exception as e:  # timeout etc.
        return -1, str(e)


def wait_inbound(token):
    """Long-poll until a real user text message arrives; return (user_id, context_token, text)."""
    cursor = ""
    print("[*] waiting for an inbound WeChat message (send one from your phone)...")
    while True:
        status, text = post(token, "getupdates", {
            "get_updates_buf": cursor,
            "base_info": {"channel_version": "esp-claw-wechat"},
        }, timeout=45)
        if status != 200:
            print(f"[!] getupdates status={status} body={text[:200]}")
            time.sleep(3)
            continue
        try:
            data = json.loads(text) if text.strip() else {}
        except json.JSONDecodeError:
            print(f"[!] getupdates unparsable body: {text[:200]}")
            time.sleep(3)
            continue
        if data.get("ret", 0) != 0 or data.get("errcode", 0) != 0:
            print(f"[!] getupdates error: {text[:300]}")
            time.sleep(3)
            continue
        cursor = data.get("get_updates_buf", cursor)
        for msg in data.get("msgs", []):
            if msg.get("message_type") != 1:
                print(f"[*] skip non-user message type={msg.get('message_type')}")
                continue
            user_id = msg.get("from_user_id") or ""
            ct = msg.get("context_token") or ""
            content = ""
            for item in msg.get("item_list", []):
                if item.get("type") == 1:
                    content = (item.get("text_item") or {}).get("text", "")
            print(f"[+] inbound from={user_id} ctx_len={len(ct)} text={content!r}")
            if user_id and ct:
                return user_id, ct, content


def send_variant(token, user_id, ct, label, base_info, extra_msg=None, typing_first=False):
    if typing_first:
        s, t = post(token, "getconfig", {
            "ilink_user_id": user_id, "context_token": ct, "base_info": base_info,
        }, timeout=15)
        ticket = ""
        try:
            ticket = json.loads(t).get("typing_ticket", "")
        except json.JSONDecodeError:
            pass
        print(f"    getconfig: status={s} ticket={'yes' if ticket else 'no'} body={t[:120]}")
        if ticket:
            s, t = post(token, "sendtyping", {
                "ilink_user_id": user_id, "typing_ticket": ticket, "status": 1,
                "base_info": base_info,
            }, timeout=15)
            print(f"    sendtyping: status={s} body={t[:120]}")
    msg = {
        "from_user_id": "",
        "to_user_id": user_id,
        "client_id": f"hosttest-{random.getrandbits(64):016x}",
        "message_type": 2,
        "message_state": 2,
        "context_token": ct,
        "item_list": [{"type": 1, "text_item": {"text": f"[{label}] 投递测试 {int(time.time())}"}}],
    }
    if extra_msg:
        msg.update(extra_msg)
    status, text = post(token, "sendmessage", {"msg": msg, "base_info": base_info}, timeout=15)
    print(f"[{label}] sendmessage status={status} body={text[:200]}")
    return status


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--token", default=os.environ.get("WECHAT_BOT_TOKEN", ""))
    args = ap.parse_args()
    if not args.token:
        print("error: pass --token ilinkbot_xxx (from device NVS claw/wechat_token)")
        return 2

    user_id, ct, _ = wait_inbound(token=args.token)

    variants = [
        # A: byte-identical to current esp-claw behavior.
        ("A-espclaw-as-is", {"channel_version": "esp-claw-wechat"}, None, False),
        # B: official openclaw-weixin 2.4.8 base_info.
        ("B-official-baseinfo", {"channel_version": "2.4.8", "bot_agent": "OpenClaw"}, None, False),
        # C: official base_info + getconfig/sendtyping first.
        ("C-official+typing", {"channel_version": "2.4.8", "bot_agent": "OpenClaw"}, None, True),
        # D: official base_info + run_id.
        ("D-official+runid", {"channel_version": "2.4.8", "bot_agent": "OpenClaw"},
         {"run_id": f"hosttest-run-{random.getrandbits(32):08x}"}, False),
    ]
    for label, base_info, extra, typing in variants:
        send_variant(args.token, user_id, ct, label, base_info, extra, typing)
        time.sleep(2)

    print("\n[*] done. Check the phone: which [label] messages actually arrived?")
    return 0


if __name__ == "__main__":
    sys.exit(main())
