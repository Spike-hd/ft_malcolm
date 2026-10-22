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

## Vérification dans un laboratoire

Lancer le programme avant de provoquer, depuis la cible, une résolution ARP pour `source_ip`. Si une association utilisable est déjà en cache, la cible peut ne pas émettre immédiatement la requête attendue.

Sur une cible Linux, le cache des voisins peut être consulté avec :

```sh
ip neigh show
```

Après acceptation de la réponse, l’entrée recherchée doit associer `source_ip` à `source_mac`.

L’envoi réussi par `sendto()` prouve seulement la remise de la trame au système local, pas son acceptation par la cible. Le programme n’affiche pas de confirmation de modification du cache distant.

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
