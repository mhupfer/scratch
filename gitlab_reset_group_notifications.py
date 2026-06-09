import requests

# --- KONFIGURATION ---
GITLAB_URL = "https://gitlab.rim.net"  # Oder eure eigene Instanz
PRIVATE_TOKEN = ""

# ---------------------


headers = {
    "PRIVATE-TOKEN": PRIVATE_TOKEN,
    "Accept": "application/json"
}

def reset_participating_groups():
    page = 1
    total_checked = 0
    total_changed = 0
    
    print("Suche nach Gruppen mit Level 'participating'...")

    while True:
        # 1. Gruppen abrufen
        groups_url = f"{GITLAB_URL}/api/v4/groups"
        params = {"min_access_level": 10, "per_page": 100, "page": page}
        
        response = requests.get(groups_url, headers=headers, params=params)
        if response.status_code != 200:
            break
        
        groups = response.json()
        if not groups:
            break

        for group in groups:
            group_id = group['id']
            group_path = group['full_path']
            print(f"Check group {group_path} (id {group_id})")
            total_checked += 1
            
            # 2. Aktuelles Benachrichtigungs-Level für diese Gruppe abfragen
            notif_url = f"{GITLAB_URL}/api/v4/notification_settings"
            notif_params = {"group_id": group_id}
            
            notif_res = requests.get(notif_url, headers=headers, params=notif_params)
            
            if notif_res.status_code == 200:
                current_level = notif_res.json().get('level')
                
                # 3. Nur ändern, wenn es exakt auf 'participating' steht
                if current_level == 'participating':
                    update_url = f"{notif_url}?group_id={group_id}&level=global"
                    put_res = requests.put(update_url, headers=headers)
                    
                    if put_res.status_code == 200:
                        print(f"✅ {group_path}: Von 'participating' auf 'global' gesetzt.")
                        total_changed += 1
                else:
                    # Optional: Zeigt an, was übersprungen wurde
                    # print(f"ℹ️ {group_path} übersprungen (aktuell: {current_level})")
                    pass

        # Pagination
        if 'X-Next-Page' in response.headers and response.headers['X-Next-Page']:
            page = response.headers['X-Next-Page']
        else:
            break

    print(f"\nAbgeschlossen!")
    print(f"Geprüfte Gruppen: {total_checked}")
    print(f"Geänderte Gruppen: {total_changed}")

if __name__ == "__main__":
    reset_participating_groups()
