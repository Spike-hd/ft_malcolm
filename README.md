# ft_malcolm

Programme pédagogique en C illustrant l’usurpation ARP (*ARP spoofing*) sur un réseau local Ethernet/IPv4.

Le comportement attendu est d’attendre une requête ARP précise, d’envoyer **une seule réponse usurpée**, puis de quitter. Le programme ne réalise ni interception complète du trafic ni empoisonnement continu.

> À utiliser uniquement dans un laboratoire isolé ou sur un réseau pour lequel on dispose d’une autorisation explicite. Une fausse association ARP peut interrompre les communications de la cible.

## Principe

ARP (*Address Resolution Protocol*) permet de trouver la MAC associée à une adresse IPv4 sur un réseau local.

Lorsqu’une machine ne connaît pas cette association, elle peut envoyer une requête Ethernet en broadcast :

```text
Cible : « Qui possède 192.168.1.10 ? »
```

`ft_malcolm` attend cette demande et répond avec l’association choisie :

```text
ft_malcolm : « 192.168.1.10 correspond à 02:00:00:00:00:30. »
```

ARP ne fournit pas de mécanisme d’authentification de cette annonce. Si la cible accepte la réponse, son cache ARP peut enregistrer :

```text
192.168.1.10 → 02:00:00:00:00:30
```

Elle peut alors envoyer à cette MAC ses prochaines trames destinées à l’IP annoncée.

## Prérequis

- **Linux** : le code utilise les sockets `AF_PACKET` et `struct sockaddr_ll`.
- Un compilateur C accessible via `gcc`, `make` et les en-têtes de développement Linux.
- Les privilèges nécessaires à l’ouverture d’un socket brut : généralement `root`, ou la capacité Linux `CAP_NET_RAW`.
- Une interface Ethernet permettant la réception et l’émission des trames nécessaires.
- Un accès au même domaine de broadcast Ethernet que la cible, typiquement le même réseau local ou VLAN.

Cette implémentation ne compile pas nativement sur macOS, même si le dépôt peut y être édité. Utiliser un environnement Linux disposant d’un accès adapté au réseau de test.

## Compilation

```sh
make
```

Le binaire généré est `ft_malcolm`. Les options de compilation sont `-Wall -Wextra -Werror`.

| Commande | Action |
|---|---|
| `make` | Compile le programme |
| `make clean` | Supprime les fichiers objets |
| `make fclean` | Supprime les objets et le binaire |
| `make re` | Nettoie puis recompile |

## Utilisation

```sh
sudo ./ft_malcolm <source_ip> <source_mac> <target_ip> <target_mac>
```

Les quatre arguments doivent être fournis dans cet ordre :

| Argument | Signification |
|---|---|
| `source_ip` | IPv4 à usurper : l’adresse recherchée dans la requête ARP |
| `source_mac` | MAC à annoncer comme étant associée à cette IPv4 |
| `target_ip` | IPv4 de la machine qui émet la requête |
| `target_mac` | MAC de cette machine cible |

`source_ip` n’est pas nécessairement l’IP de la machine exécutant le programme. `source_mac` est la MAC à annoncer : elle peut être celle de cette machine, mais le programme ne l’impose pas.

Les IPv4 doivent être écrites en notation décimale pointée. Les MAC doivent respecter le format `XX:XX:XX:XX:XX:XX` ; les lettres hexadécimales peuvent être en minuscules ou majuscules.

### Exemple de laboratoire

```sh
sudo ./ft_malcolm 192.168.1.10 02:00:00:00:00:30 192.168.1.20 02:00:00:00:00:20
```

Cet exemple demande au programme :

1. D’attendre que la machine `192.168.1.20`, de MAC `02:00:00:00:00:20`, demande en broadcast qui possède `192.168.1.10`.
2. De lui répondre que cette IP correspond à `02:00:00:00:00:30`.
3. De quitter après l’envoi de la réponse.

Les adresses de cet exemple sont illustratives et doivent être adaptées au laboratoire.

## Déroulement attendu

1. Vérifier les quatre arguments.
2. Installer le gestionnaire de `Ctrl+C`.
3. Ouvrir un socket brut limité au protocole ARP.
4. Recevoir les trames et sélectionner une requête qui présente :
   - un format Ethernet/ARP/IPv4 valide et une longueur suffisante ;
   - une opération ARP égale à `1` — requête ;
   - une destination Ethernet broadcast : `ff:ff:ff:ff:ff:ff` ;
   - une IP et une MAC émettrices correspondant à la cible ;
   - une IP recherchée correspondant à `source_ip`.
5. Construire une réponse ARP avec l’opération `2`, annonçant `source_ip → source_mac`.
6. L’envoyer à la cible sur l’interface ayant reçu la requête.
7. Fermer le socket et quitter.

Le délai de réception est de deux secondes par appel. Son expiration fait reprendre l’attente : ce n’est pas une limite de durée totale du programme. `Ctrl+C` permet d’arrêter cette attente.

### Format de la réponse

```text
En-tête Ethernet : 14 octets
Message ARP      : 28 octets
Bourrage         : 18 octets
Total envoyé     : 60 octets, hors FCS Ethernet
```

Le message ARP contient deux MAC de 6 octets, deux IPv4 de 4 octets et 8 octets de champs descriptifs. Le programme laisse l’ajout du FCS à l’interface.

## Tester avec une seule VM Linux dans UTM

Cette procédure utilise un **namespace réseau Linux** pour simuler la cible dans la VM. Il n’est pas nécessaire de créer une deuxième VM ni de modifier la configuration réseau d’UTM.

Toutes les commandes sont à exécuter **dans la VM**, sauf celles explicitement indiquées comme étant à lancer sur le Mac.

### 1. Préparer les outils

Sur Debian ou Ubuntu, si nécessaire :

```sh
sudo apt update
sudo apt install build-essential iproute2 iputils-ping tcpdump
```

Depuis le dossier du projet :

```sh
make
```

### 2. Comprendre le réseau de test

```text
VM Linux
│
├── Namespace principal : ft_malcolm
│   Interface : ft-host
│   IPv4      : 10.203.0.1/24
│   MAC       : 02:00:00:00:00:10
│       │
│       │ Paire veth : câble Ethernet virtuel
│       │
└── Namespace ft-target : cible
    Interface : ft-peer
    IPv4      : 10.203.0.2/24
    MAC       : 02:00:00:00:00:20

IP à usurper : 10.203.0.99, attribuée à aucune interface
```

Un namespace possède ses propres interfaces, routes et cache ARP, tout en partageant le noyau de la VM. Une paire `veth` relie les deux environnements : les trames envoyées à une extrémité arrivent à l’autre.

`ft-host` est une interface **dans Linux**, pas une interface du Mac. Ce laboratoire ne crée ni pont ni transfert vers `enp0s1` et ne modifie pas le réseau habituel de la VM. Vérifier que `10.203.0.0/24` n’est pas déjà utilisé par un autre réseau de la VM avant de le créer.

### 3. Créer le réseau isolé

Exécuter ces commandes **une seule fois par création du laboratoire** :

```sh
sudo ip netns add ft-target

sudo ip link add ft-host type veth peer name ft-peer
sudo ip link set ft-peer netns ft-target

sudo ip link set ft-host address 02:00:00:00:00:10
sudo ip addr add 10.203.0.1/24 dev ft-host
sudo ip link set ft-host up

sudo ip netns exec ft-target ip link set lo up
sudo ip netns exec ft-target ip link set ft-peer address 02:00:00:00:00:20
sudo ip netns exec ft-target ip addr add 10.203.0.2/24 dev ft-peer
sudo ip netns exec ft-target ip link set ft-peer up
```

- `ip netns add` crée le namespace cible.
- `ip link add ... type veth` crée les deux extrémités du câble virtuel.
- `ip link set ... netns` déplace une interface dans le namespace cible.
- `ip link set ... address` fixe une MAC connue pour le test.
- `ip addr add` attribue une IPv4 et son préfixe réseau.
- `ip link set ... up` active une interface.
- `ip netns exec ft-target ...` exécute une commande dans le réseau de la cible.

Vérifier la configuration :

```sh
ip -br addr show ft-host
sudo ip netns list
sudo ip netns exec ft-target ip -br addr
```

Vérifier le lien avec un ping vers l’IP réelle du côté principal :

```sh
sudo ip netns exec ft-target ping -c 1 10.203.0.1
```

Ce ping doit normalement réussir.

> `ft-target` est un **namespace**, pas une interface : `ip -br addr show ft-target` affiche donc une erreur. L’interface de la cible s’appelle `ft-peer` et se consulte depuis son namespace. Le réseau reste présent après la sortie du programme, mais n’est pas conservé après un redémarrage de la VM.

### 4. Démarrer la capture — terminal 1 de la VM

```sh
sudo tcpdump -i ft-host -s 0 -U -w - arp > ~/ft_malcolm.pcap
```

Laisser la capture tourner pendant le test.

- `-i ft-host` sélectionne le câble virtuel du laboratoire.
- `-s 0` conserve les paquets sans les tronquer à une petite taille de capture.
- `-U` écrit les paquets au fil de la capture.
- `-w -` écrit la capture binaire sur la sortie standard.
- `arp` limite la capture aux trames ARP.
- `>` enregistre cette sortie dans un fichier, ouvert par le shell de l’utilisateur dans son dossier personnel.

Cette méthode évite notamment les problèmes de droits rencontrés lorsque `tcpdump` tente d’ouvrir directement un fichier existant dans `/tmp` après avoir abandonné ses privilèges. Elle **écrase** une ancienne capture au même emplacement : la sauvegarder avant de recommencer si nécessaire.

### 5. Démarrer le programme — terminal 2 de la VM

Depuis le dossier du projet :

```sh
sudo ./ft_malcolm 10.203.0.99 02:00:00:00:00:10 10.203.0.2 02:00:00:00:00:20
```

Le programme attend que `10.203.0.2` recherche `10.203.0.99`, pour lui annoncer la MAC `02:00:00:00:00:10`.

Il peut rester silencieux pendant l’attente. Le délai de réception de deux secondes ne doit pas le faire quitter.

### 6. Déclencher la requête — terminal 3 de la VM

Si un test a déjà été réalisé, supprimer uniquement son éventuelle entrée ARP :

```sh
sudo ip netns exec ft-target ip neigh del 10.203.0.99 dev ft-peer
```

Si l’entrée n’existe pas, le message d’erreur n’est pas gênant.

Déclencher ensuite la résolution ARP :

```sh
sudo ip netns exec ft-target ping -c 1 -W 2 10.203.0.99
```

**Le ping peut échouer même si le test ARP réussit.** Le programme annonce une association, mais ne configure pas `10.203.0.99` sur une interface et ne répond pas à ICMP pour cette adresse.

### 7. Vérifier le résultat

Dans le terminal 3, consulter le cache ARP de la cible :

```sh
sudo ip netns exec ft-target ip neigh show 10.203.0.99 dev ft-peer
```

Le résultat attendu contient :

```text
10.203.0.99 lladdr 02:00:00:00:00:10
```

L’état de l’entrée peut évoluer (`REACHABLE`, `STALE`, etc.). L’important est l’association IP/MAC ; la vérifier rapidement après le test.

Dans le terminal 2, le programme doit avoir quitté après une réponse. Vérifier son statut immédiatement, avant toute autre commande :

```sh
echo $?
```

Le statut attendu est `0`.

Dans le terminal 1, arrêter `tcpdump` avec `Ctrl+C`. La capture est disponible dans `~/ft_malcolm.pcap` **dans la VM**.

L’envoi réussi par `sendto()` prouve seulement la remise de la trame au système local, pas son acceptation par la cible. La capture et le cache ARP permettent de vérifier le résultat du test.

### 8. Ouvrir la capture dans Wireshark sur le Mac

Le trafic du laboratoire reste à l’intérieur de la VM : capturer le Wi-Fi du Mac ne permet pas de l’observer. Il faut récupérer la capture effectuée sur `ft-host`.

Deux possibilités :

- Copier le fichier via un dossier partagé UTM déjà configuré.
- Utiliser `scp` si un serveur SSH est actif dans la VM et accessible depuis le Mac.

Pour la deuxième méthode, relever dans la VM le nom d’utilisateur et l’IPv4 de son interface UTM :

```sh
whoami
ip -br addr show enp0s1
```

**Dans un terminal local du Mac, pas dans la console UTM ni dans une session SSH vers Debian**, exécuter cette commande en adaptant l’utilisateur, l’adresse et le chemin :

```sh
scp debian@192.168.64.11:/home/debian/ft_malcolm.pcap ~/Downloads/
```

Le chemin de destination `~/Downloads/` appartient à la machine qui exécute `scp`. Si la commande est lancée dans Debian, le fichier sera copié dans Debian, pas sur le Mac.

À la première connexion, vérifier l’empreinte SSH avant d’accepter la clé, puis saisir le mot de passe de l’utilisateur de la VM si demandé.

Sur le Mac, afficher le fichier dans le Finder :

```sh
open -R ~/Downloads/ft_malcolm.pcap
```

L’ouvrir avec Wireshark et utiliser le filtre d’affichage :

```text
arp
```

Les deux échanges attendus sont :

```text
Who has 10.203.0.99? Tell 10.203.0.2
10.203.0.99 is at 02:00:00:00:00:10
```

Inspecter les en-têtes :

| Champ | Requête | Réponse |
|---|---|---|
| Destination Ethernet | `ff:ff:ff:ff:ff:ff` | `02:00:00:00:00:20` |
| Source Ethernet | `02:00:00:00:00:20` | `02:00:00:00:00:10` |
| Opération ARP | `1` | `2` |
| IP émettrice ARP | `10.203.0.2` | `10.203.0.99` |
| IP cible ARP | `10.203.0.99` | `10.203.0.2` |

Filtres utiles : `arp.opcode == 1` pour les requêtes et `arp.opcode == 2` pour les réponses. Il peut y avoir des retransmissions de requêtes ; le programme ne doit envoyer qu’une réponse par exécution.

### 9. Refaire le test et vérifier les autres comportements

Pour recommencer, conserver le réseau, démarrer une nouvelle capture si souhaité, relancer le programme, supprimer l’entrée ARP de test, puis relancer le ping. Il n’est pas nécessaire de recréer le namespace.

| Test | Résultat attendu |
|---|---|
| Arguments manquants ou adresses invalides | Message d’erreur et statut `1` |
| Aucune requête | Le programme reste en attente au-delà de deux secondes |
| `Ctrl+C` pendant l’attente | Arrêt du programme et fermeture du socket |
| Requête pour une autre IP | Aucune réponse ; le programme continue d’attendre |
| Requête attendue | Une réponse, puis arrêt avec statut `0` |

Pour générer une requête pour une autre IP inutilisée du laboratoire :

```sh
sudo ip netns exec ft-target ping -c 1 -W 2 10.203.0.98
```

### 10. Nettoyer le laboratoire

Arrêter d’abord les captures et les programmes du test, puis exécuter dans la VM :

```sh
sudo ip link del ft-host
sudo ip netns del ft-target
```

La suppression de `ft-host` supprime aussi l’autre extrémité de la paire veth. Ces commandes ne modifient pas `enp0s1`.

Après avoir récupéré la capture, supprimer éventuellement sa copie dans la VM :

```sh
rm -i ~/ft_malcolm.pcap
```

Si une ancienne capture a été placée dans un dossier créé avec `mktemp`, ce dossier peut aussi être supprimé une fois son contenu récupéré. Vérifier son chemin exact : ne pas supprimer `/tmp` lui-même.

### Dépannage

| Symptôme | Vérification |
|---|---|
| `File exists` lors de la création | Le laboratoire existe peut-être déjà : consulter `sudo ip netns list` et `ip -br addr show ft-host` |
| `Device "ft-target" does not exist` | Utiliser `sudo ip netns exec ft-target ip -br addr` : c’est un namespace, pas une interface |
| Aucune requête visible | Vérifier l’interface de capture et supprimer l’entrée ARP de test avant le ping |
| Requête visible mais aucune réponse | Vérifier les arguments et le filtre de l’opération : il doit accepter `00 01` |
| Erreur d’ouverture du socket | Lancer le programme avec `sudo` dans Linux |
| Programme arrêté au bout de deux secondes | Vérifier que `EAGAIN` et `EWOULDBLOCK` relancent la réception |
| `Permission denied` sur la capture | Utiliser la redirection vers le dossier personnel indiquée à l’étape 4 et vérifier les droits d’un éventuel fichier existant |
| Fichier absent du Mac après `scp` | Vérifier que `scp` a été lancé dans un terminal local du Mac et non dans la VM |
| `scp` refuse la connexion | Vérifier le serveur SSH de la VM, son adresse et son accessibilité ; utiliser sinon un dossier partagé UTM |
| Namespace absent après redémarrage | Recréer le laboratoire : ces commandes ne rendent pas la configuration persistante |

## Limites

- IPv4 uniquement ; IPv6 utilise NDP et non ARP.
- Trames Ethernet sans en-tête VLAN explicite dans le buffer analysé.
- Une seule réponse : aucune mise à jour périodique ni garantie de maintien de l’association.
- Pas de sélection d’interface en argument : la réponse utilise l’interface de réception.
- Aucun transfert des paquets détournés, aucune analyse du trafic et aucune restauration automatique du cache à la sortie.
- La machine légitime peut aussi répondre ; le résultat dépend du cache de la cible, des protections réseau et des réponses reçues.
- Les communications peuvent être interrompues si la MAC annoncée ne permet pas d’atteindre le véritable destinataire.

## Organisation du dépôt

```text
.
├── Makefile
├── README.md
├── project.md             # Énoncé
├── includes/
│   └── ft_malcolm.h        # En-têtes et prototypes
└── src/
    ├── main.c             # Socket, signaux et boucle de réception
    ├── parsing.c          # Validation des arguments et filtrage des trames
    └── arp.c              # Construction et envoi de la réponse
```
