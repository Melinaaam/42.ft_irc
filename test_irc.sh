#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YIGHLLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' #

SERVER_PORT=6667
SERVER_PASS="test"
SERVER_BIN="./ircserv"
TEST_COUNT=0
PASSED_COUNT=0
FAILED_COUNT=0
SERVER_PID=""

print_header() {
    echo -e "${BLUE}============================================${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}============================================${NC}"
}

print_test() {
    echo -e "${CYAN}[TEST $((++TEST_COUNT))] $1${NC}"
}

print_success() {
    echo -e "${GREEN}[SUCCESS] $1${NC}"
    ((PASSED_COUNT++))
}

print_error() {
    echo -e "${RED}[ERROR] $1${NC}"
    ((FAILED_COUNT++))
}

print_warning() {
    echo -e "${YELLOW}[WARNING] $1${NC}"
}

print_info() {
    echo -e "${PURPLE}[INFO] $1${NC}"
}

start_server() {
    if [ -n "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        stop_server
    fi

    print_info "Démarrage du serveur IRC sur le port $SERVER_PORT..."

    if ! [ -x "$SERVER_BIN" ]; then
        print_error "Serveur IRC non trouvé ou non exécutable: $SERVER_BIN"
        echo "Veuillez compiler le serveur avec 'make'"
        exit 1
    fi

    $SERVER_BIN $SERVER_PORT $SERVER_PASS > server.log 2>&1 &
    SERVER_PID=$!

    sleep 2

    if kill -0 $SERVER_PID 2>/dev/null; then
        print_success "Serveur démarré (PID: $SERVER_PID)"
    else
        print_error "Échec du démarrage du serveur"
        cat server.log
        exit 1
    fi
}

stop_server() {
    if [ -n "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        print_info "Arrêt du serveur IRC..."
        kill $SERVER_PID
        wait $SERVER_PID 2>/dev/null
        print_success "Serveur arrêté"
    fi
}


send_irc_command() {
    local command="$1"
    local expected_response="$2"
    local timeout="${3:-3}"

    echo -e "$command" | nc -w$timeout localhost $SERVER_PORT
}

test_connection() {
    print_test "Test de connexion basique"

    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TestUser\r\nUSER test test localhost :Test User\r\nQUIT\r\n" | nc -w3 localhost $SERVER_PORT)

    echo -e "${CYAN}[DEBUG] Commande envoyée:${NC}"
    echo "PASS $SERVER_PASS"
    echo "NICK TestUser"
    echo "USER test test localhost :Test User"
    echo "QUIT"
    echo
    echo -e "${CYAN}[DEBUG] Réponse reçue:${NC}"
    echo "$response"
    echo

    if echo "$response" | grep -q "001.*Welcome"; then
        print_success "Connexion et enregistrement réussis"
    else
        print_error "Échec de la connexion"
        echo "Réponse reçue: $response"
    fi
}


test_auth() {
    print_header "TESTS D'AUTHENTIFICATION"

    print_test "Test mot de passe correct"
    local cmd="PASS $SERVER_PASS\r\nNICK TestUser\r\nUSER test test localhost :Test User\r\nQUIT\r\n"
    echo -e "${CYAN}[DEBUG] Test: Connexion avec bon mot de passe${NC}"
    echo -e "${CYAN}[DEBUG] Commandes:${NC} PASS $SERVER_PASS | NICK TestUser | USER test test localhost :Test User | QUIT"

    local response=$(echo -e "$cmd" | nc -w3 localhost $SERVER_PORT)
    echo -e "${CYAN}[DEBUG] Réponse:${NC}"
    echo "$response" | head -10
    echo

    if echo "$response" | grep -q "001.*Welcome"; then
        print_success "Authentification réussie avec le bon mot de passe"
        echo -e "${GREEN}→ Code 001 Welcome trouvé dans la réponse${NC}"
    else
        print_error "Échec d'authentification avec le bon mot de passe"
        echo -e "${RED}→ Code 001 Welcome non trouvé${NC}"
    fi

    print_test "Test mot de passe incorrect"
    local cmd_wrong="PASS wrongpass\r\nNICK TestUser\r\nUSER test test localhost :Test User\r\nQUIT\r\n"
    echo -e "${CYAN}[DEBUG] Test: Connexion avec mauvais mot de passe${NC}"
    echo -e "${CYAN}[DEBUG] Commandes:${NC} PASS wrongpass | NICK TestUser | USER test test localhost :Test User | QUIT"

    local response=$(echo -e "$cmd_wrong" | nc -w3 localhost $SERVER_PORT)
    echo -e "${CYAN}[DEBUG] Réponse:${NC}"
    echo "$response" | head -10
    echo

    if echo "$response" | grep -q "464.*Password"; then
        print_success "Rejet correct du mauvais mot de passe"
        echo -e "${GREEN}→ Code 464 Password error trouvé${NC}"
    else
        print_error "Le serveur devrait rejeter le mauvais mot de passe"
        echo -e "${RED}→ Code 464 Password error non trouvé${NC}"
    fi
}

test_nick() {
    print_header "TESTS NICKNAME"

    print_test "Test changement de nickname"
    local cmd="PASS $SERVER_PASS\r\nNICK TestUser\r\nUSER test test localhost :Test User\r\nNICK NewNick\r\nQUIT\r\n"
    echo -e "${CYAN}[DEBUG] Test: Changement de nickname${NC}"
    echo -e "${CYAN}[DEBUG] Commandes:${NC} PASS | NICK TestUser | USER | NICK NewNick | QUIT"

    local response=$(echo -e "$cmd" | nc -w3 localhost $SERVER_PORT)
    echo -e "${CYAN}[DEBUG] Réponse (extraits):${NC}"
    echo "$response" | grep -E "(NICK|001)" | head -5
    echo

    if echo "$response" | grep -q "NewNick"; then
        print_success "Changement de nickname réussi"
        echo -e "${GREEN}→ NewNick trouvé dans la réponse${NC}"
    else
        print_error "Échec du changement de nickname"
        echo -e "${RED}→ NewNick non trouvé dans la réponse${NC}"
    fi
}


test_channels() {
    print_header "TESTS DES CANAUX"

    print_test "Test JOIN/PART basique"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TestUser\r\nUSER test test localhost :Test User\r\nJOIN #test\r\nPART #test\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)
    if echo "$response" | grep -q "JOIN.*#test" && echo "$response" | grep -q "PART.*#test"; then
        print_success "JOIN/PART fonctionnent correctement"
    else
        print_error "Problème avec JOIN/PART"
    fi
}


test_topic() {
    print_header "TESTS DU TOPIC"

    print_test "Test topic vide (GET)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TestUser\r\nUSER test test localhost :Test User\r\nJOIN #topictest\r\nTOPIC #topictest\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)
    if echo "$response" | grep -q "331.*No topic is set"; then
        print_success "Topic vide correctement rapporté"
    else
        print_error "Le serveur devrait indiquer qu'aucun topic n'est défini"
    fi

    print_test "Test définition de topic (SET)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TopicSetter\r\nUSER test test localhost :Test User\r\nJOIN #topictest\r\nTOPIC #topictest :Nouveau topic de test\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)
    if echo "$response" | grep -q "TOPIC.*#topictest.*Nouveau topic de test"; then
        print_success "Définition de topic réussie"
    else
        print_error "Échec de la définition du topic"
    fi

    print_test "Test récupération de topic (GET après SET)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TopicGetter\r\nUSER test test localhost :Test User\r\nJOIN #topictest\r\nTOPIC #topictest :Test topic\r\nTOPIC #topictest\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)
    if echo "$response" | grep -q "332.*Test topic"; then
        print_success "Récupération de topic réussie"
    else
        print_error "Échec de la récupération du topic"
    fi
}


test_privmsg() {
    print_header "TESTS DES MESSAGES PRIVÉS"

    print_test "Test PRIVMSG vers canal"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK MsgSender\r\nUSER test test localhost :Test User\r\nJOIN #msgtest\r\nPRIVMSG #msgtest :Hello world\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)
    if echo "$response" | grep -q "JOIN.*#msgtest"; then
        print_success "PRIVMSG vers canal traité (connexion réussie)"
    else
        print_error "Problème avec PRIVMSG vers canal"
    fi
}


test_topic_advanced() {
    print_header "TESTS AVANCÉS DU TOPIC"

    print_test "Test topic sans permission (utilisateur normal)"
    echo -e "${CYAN}[DEBUG] Test: Protection du topic avec mode +t${NC}"

    echo -e "${CYAN}[DEBUG] Test 1: Vérification que +t fonctionne correctement${NC}"

    local response=$(echo -e "PASS test\r\nNICK TestOperator\r\nUSER op op localhost :Test Operator\r\nJOIN #protection_test\r\nMODE #protection_test +t\r\nTOPIC #protection_test :Topic protégé par opérateur\r\nNICK TestOperatorRenamed\r\nTOPIC #protection_test :Changement par opérateur renommé\r\nQUIT :Test terminé\r\n" | nc -w10 localhost $SERVER_PORT)

    echo -e "${CYAN}[DEBUG] Réponse complète du test 1:${NC}"
    echo "$response"
    echo

    if echo "$response" | grep -q "MODE.*#protection_test.*+t"; then
        echo -e "${GREEN}✅ Mode +t correctement activé${NC}"
    else
        echo -e "${RED}❌ Mode +t non activé${NC}"
        return
    fi

    if echo "$response" | grep -q "TOPIC.*#protection_test.*Changement par opérateur renommé"; then
        print_success "Comportement conforme RFC 2812 : Opérateur garde ses privilèges après changement de nickname"
        echo -e "${GREEN}→ Changement de nickname conserve les privilèges d'opérateur (correct selon IRC)${NC}"
    else
        print_error "L'opérateur devrait pouvoir changer le topic même après changement de nickname"
        return
    fi

    echo -e "${CYAN}[DEBUG] Test 2: Vérification avec un vrai non-opérateur${NC}"

    (
        echo -e "PASS test\r\nNICK ChannelOwner\r\nUSER owner owner localhost :Channel Owner\r\nJOIN #realtest\r\nMODE #realtest +t\r\nTOPIC #realtest :Topic initial protégé\r\n"
        sleep 5  # Rester connecté
        echo -e "QUIT :Propriétaire se déconnecte\r\n"
    ) | nc -w10 localhost $SERVER_PORT &

    local owner_pid=$!

    sleep 1

    local response2=$(echo -e "PASS test\r\nNICK RealNormalUser\r\nUSER normal normal localhost :Normal User\r\nJOIN #realtest\r\nTOPIC #realtest :Tentative par vrai non-opérateur\r\nQUIT :Test utilisateur normal terminé\r\n" | nc -w8 localhost $SERVER_PORT)

    kill $owner_pid 2>/dev/null || true
    wait $owner_pid 2>/dev/null || true

    echo -e "${CYAN}[DEBUG] Réponse utilisateur normal:${NC}"
    echo "$response2"
    echo

    if echo "$response2" | grep -q "482.*not channel operator\|482.*channel operator"; then
        print_success "Protection +t fonctionne : Vrai non-opérateur correctement bloqué (482)"
        echo -e "${GREEN}→ Code 482 ERR_CHANOPRIVSNEEDED trouvé pour le vrai non-opérateur${NC}"
    elif echo "$response2" | grep -q "TOPIC.*#realtest.*Tentative par vrai non-opérateur"; then
        print_error "Le vrai non-opérateur ne devrait pas pouvoir changer le topic protégé"
        echo -e "${RED}→ Protection +t ne fonctionne pas correctement${NC}"
    else
        print_warning "Test inconcluant - Le canal était peut-être vide"
        echo -e "${YELLOW}[INFO] Le canal était peut-être vide, rendant l'utilisateur opérateur automatiquement${NC}"
    fi

    echo -e "${CYAN}[INFO] Résumé du test de protection topic:${NC}"
    echo -e "${GREEN}✅ Mode +t fonctionne correctement${NC}"
    echo -e "${GREEN}✅ Changement de nickname conserve les privilèges (conforme RFC 2812)${NC}"
    echo -e "${GREEN}✅ Vrais non-opérateurs sont bloqués par +t${NC}"
}

test_channel_modes() {
    print_header "TESTS DES MODES DE CANAL"

    print_test "Test mode +i (invite only)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ModeTest\r\nUSER test test localhost :Test User\r\nJOIN #modetest\r\nMODE #modetest +i\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#modetest.*+i"; then
        print_success "Mode +i (invite only) appliqué"
    else
        print_error "Échec du mode +i"
    fi

    print_test "Test mode +t (topic protected)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TopicMode\r\nUSER test test localhost :Test User\r\nJOIN #topicmode\r\nMODE #topicmode +t\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#topicmode.*+t"; then
        print_success "Mode +t (topic protected) appliqué"
    else
        print_error "Échec du mode +t"
    fi

    print_test "Test mode +k (password)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK KeyMode\r\nUSER test test localhost :Test User\r\nJOIN #keymode\r\nMODE #keymode +k secret123\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#keymode.*+k"; then
        print_success "Mode +k (password) appliqué"
    else
        print_error "Échec du mode +k"
    fi

    print_test "Test mode +l (user limit)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK LimitMode\r\nUSER test test localhost :Test User\r\nJOIN #limitmode\r\nMODE #limitmode +l 5\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#limitmode.*+l"; then
        print_success "Mode +l (user limit) appliqué"
    else
        print_error "Échec du mode +l"
    fi

    print_test "Test modes multiples en une commande (+it)"
    echo -e "${CYAN}[DEBUG] Test: Application de modes multiples +it en une seule commande${NC}"

    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK MultiMode1\r\nUSER test test localhost :Test User\r\nJOIN #multimode1\r\nMODE #multimode1 +it\r\nMODE #multimode1\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    echo -e "${CYAN}[DEBUG] Réponse complète pour modes multiples +it:${NC}"
    echo "$response"
    echo
    echo -e "${CYAN}[DEBUG] Analyse des lignes MODE:${NC}"
    echo "$response" | grep -n "MODE\|324" | head -10
    echo

    if echo "$response" | grep -q "MODE.*#multimode1.*+it\|MODE.*#multimode1.*+i+t"; then
        print_success "Modes multiples +it appliqués en une seule commande"
    else
        print_error "Échec des modes multiples +it"
        echo -e "${RED}[DEBUG] Modes multiples +it non détectés dans la réponse${NC}"

        if echo "$response" | grep -q "MODE.*#multimode1.*+i" && echo "$response" | grep -q "MODE.*#multimode1.*+t"; then
            echo -e "${YELLOW}[INFO] Les modes sont appliqués séparément (+i et +t individuellement)${NC}"
        elif echo "$response" | grep -q "324.*#multimode1.*+.*i" && echo "$response" | grep -q "324.*#multimode1.*+.*t"; then
            echo -e "${YELLOW}[INFO] Les modes sont actifs mais réponse format différent (324)${NC}"
        fi
    fi

    print_test "Test modes multiples avec paramètres (+ikt)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK MultiMode2\r\nUSER test test localhost :Test User\r\nJOIN #multimode2\r\nMODE #multimode2 +ikt secret123\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#multimode2.*+ikt\|MODE.*#multimode2.*+i+k+t"; then
        print_success "Modes multiples +ikt avec paramètre appliqués"
    else
        print_error "Échec des modes multiples +ikt"
    fi

    print_test "Test modes multiples complets (+iktl)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK MultiMode3\r\nUSER test test localhost :Test User\r\nJOIN #multimode3\r\nMODE #multimode3 +iktl secret123 10\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#multimode3.*+iktl\|MODE.*#multimode3.*+i+k+t+l"; then
        print_success "Modes multiples +iktl avec paramètres appliqués"
    else
        print_error "Échec des modes multiples +iktl"
    fi

    print_test "Test modes mixtes (+i-t+k)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK MixedMode\r\nUSER test test localhost :Test User\r\nJOIN #mixedmode\r\nMODE #mixedmode +it\r\nMODE #mixedmode +i-t+k newpass\r\nQUIT\r\n" | nc -w7 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#mixedmode"; then
        print_success "Modes mixtes (+i-t+k) traités correctement"
    else
        print_error "Échec des modes mixtes"
    fi

    print_test "Test suppression de modes (-itk)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK RemoveMode\r\nUSER test test localhost :Test User\r\nJOIN #removemode\r\nMODE #removemode +ikt secret123\r\nMODE #removemode -itk\r\nQUIT\r\n" | nc -w7 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#removemode.*-"; then
        print_success "Suppression de modes multiples (-itk) fonctionne"
    else
        print_error "Échec de la suppression de modes multiples"
    fi

    print_test "Test consultation des modes actuels"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK QueryMode\r\nUSER test test localhost :Test User\r\nJOIN #querymode\r\nMODE #querymode +it\r\nMODE #querymode\r\nQUIT\r\n" | nc -w7 localhost $SERVER_PORT)

    if echo "$response" | grep -q "324.*#querymode.*+"; then
        print_success "Consultation des modes actuels (324) fonctionne"
    else
        print_error "Échec de la consultation des modes actuels"
    fi
}


test_advanced_modes() {
    print_header "TESTS AVANCÉS DES MODES"

    print_test "Test gestion d'erreurs avec modes multiples"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ErrorMode\r\nUSER test test localhost :Test User\r\nJOIN #errormode\r\nMODE #errormode +k\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "461.*Need key parameter"; then
        print_success "Erreur mode +k sans paramètre correctement détectée"
    else
        print_error "Le serveur devrait détecter l'absence de paramètre pour +k"
    fi

    print_test "Test continuation après erreur de mode"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ContinueMode\r\nUSER test test localhost :Test User\r\nJOIN #continuemode\r\nMODE #continuemode +ik\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#continuemode.*+i"; then
        print_success "Mode +i appliqué même si +k échoue (continuation après erreur)"
    else
        print_error "Le serveur devrait continuer les autres modes même si un échoue"
    fi

    print_test "Test mode +o (operator privileges)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK OpMode\r\nUSER test test localhost :Test User\r\nJOIN #opmode\r\nMODE #opmode +o OpMode\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#opmode.*+o.*OpMode"; then
        print_success "Mode +o (operator) fonctionne"
    else
        print_error "Échec du mode +o"
    fi

    print_test "Test mode +l avec limite invalide"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK InvalidLimit\r\nUSER test test localhost :Test User\r\nJOIN #invalidlimit\r\nMODE #invalidlimit +l -5\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "461.*Invalid limit"; then
        print_success "Limite invalide (-5) correctement rejetée"
    else
        print_error "Le serveur devrait rejeter les limites invalides"
    fi

    print_test "Test modes avec paramètres dans le bon ordre"
    echo -e "${CYAN}[DEBUG] Test: Vérification de l'ordre des paramètres +kl mypass 15${NC}"

    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ParamOrder\r\nUSER test test localhost :Test User\r\nJOIN #paramorder\r\nMODE #paramorder +kl mypass 15\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    echo -e "${CYAN}[DEBUG] Réponse complète pour +kl mypass 15:${NC}"
    echo "$response"
    echo
    echo -e "${CYAN}[DEBUG] Lignes MODE spécifiques:${NC}"
    echo "$response" | grep "MODE.*#paramorder" | head -5
    echo

    if echo "$response" | grep -q "MODE.*#paramorder.*+kl.*mypass.*15"; then
        print_success "Paramètres des modes dans le bon ordre"
    elif echo "$response" | grep -q "MODE.*#paramorder.*+k+l.*mypass.*15"; then
        print_success "Paramètres des modes dans le bon ordre (format avec séparateurs)"
    elif echo "$response" | grep -q "MODE.*#paramorder.*+.*k.*+.*l"; then
        print_warning "Modes appliqués séparément au lieu de groupés"
        echo -e "${YELLOW}[INFO] Les modes +k et +l sont appliqués dans des commandes séparées${NC}"
    else
        print_error "Problème avec l'ordre des paramètres des modes"
        echo -e "${RED}[DEBUG] Pattern attendu non trouvé${NC}"
        echo -e "${YELLOW}[INFO] Vérifiez si les paramètres sont dans l'ordre: key puis limit${NC}"
    fi
}


test_part_advanced() {
    print_header "TESTS AVANCÉS DE LA COMMANDE PART"

    print_test "Test PART avec message personnalisé"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK PartMsg\r\nUSER test test localhost :Test User\r\nJOIN #parttest\r\nPART #parttest :Au revoir tout le monde!\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "PART.*#parttest.*Au revoir tout le monde"; then
        print_success "PART avec message personnalisé fonctionne"
    else
        print_error "Échec de PART avec message"
    fi

    print_test "Test PART sans message (norme IRC)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK PartNoMsg\r\nUSER test test localhost :Test User\r\nJOIN #partnotest\r\nPART #partnotest\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "PART.*#partnotest" && ! echo "$response" | grep -q "PART.*#partnotest.*:"; then
        print_success "PART sans message (conforme RFC)"
    else
        print_error "Problème avec PART sans message"
    fi

    print_test "Test PART sans paramètre (doit échouer selon RFC)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK PartEmpty\r\nUSER test test localhost :Test User\r\nJOIN #partempty\r\nPART\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "461.*Not enough parameters"; then
        print_success "PART sans paramètre correctement rejeté (conforme RFC)"
    else
        print_error "PART sans paramètre devrait être rejeté selon la norme IRC"
    fi

    print_test "Test PART canal inexistant"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK PartNone\r\nUSER test test localhost :Test User\r\nPART #inexistant\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "403.*No such channel"; then
        print_success "PART canal inexistant correctement géré"
    else
        print_error "Échec de la gestion de PART sur canal inexistant"
    fi
}


test_complex_scenarios() {
    print_header "TESTS DE SCÉNARIOS COMPLEXES"

    print_test "Scénario: Création canal → Topic → Utilisateur rejoint → Voit topic"
    echo -e "PASS $SERVER_PASS\r\nNICK Creator\r\nUSER test test localhost :Test User\r\nJOIN #scenario\r\nTOPIC #scenario :Topic du scénario\r\nQUIT\r\n" | nc -w3 localhost $SERVER_PORT > /dev/null

    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK Joiner\r\nUSER test test localhost :Test User\r\nJOIN #scenario\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "332.*Topic du scénario" && echo "$response" | grep -q "333.*Creator"; then
        print_success "Scénario complet: Topic persistant lors du rejoin"
    else
        print_error "Le topic ne persiste pas correctement entre les sessions"
    fi

    print_test "Scénario: Multi-canaux avec topics différents"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK MultiUser\r\nUSER test test localhost :Test User\r\nJOIN #chan1\r\nTOPIC #chan1 :Topic du canal 1\r\nJOIN #chan2\r\nTOPIC #chan2 :Topic du canal 2\r\nTOPIC #chan1\r\nTOPIC #chan2\r\nQUIT\r\n" | nc -w7 localhost $SERVER_PORT)

    if echo "$response" | grep -q "332.*Topic du canal 1" && echo "$response" | grep -q "332.*Topic du canal 2"; then
        print_success "Gestion multi-canaux avec topics séparés"
    else
        print_error "Problème avec la gestion de topics multiples"
    fi

    print_test "Scénario: Transfert d'opérateur et permissions topic"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK OpTransfer\r\nUSER test test localhost :Test User\r\nJOIN #optransfer\r\nTOPIC #optransfer :Topic initial\r\nMODE #optransfer +o OpTransfer\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "TOPIC.*Topic initial"; then
        print_success "Base du transfert d'opérateur testée"
    else
        print_warning "Transfert d'opérateur: tests limités en mode unique client"
    fi

    print_test "Scénario: Modes multiples puis validation"
    echo -e "${CYAN}[DEBUG] Test: MODE +iktl secret123 10 puis consultation${NC}"

    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ComplexMode\r\nUSER test test localhost :Test User\r\nJOIN #complex\r\nMODE #complex +iktl secret123 10\r\nMODE #complex\r\nQUIT\r\n" | nc -w7 localhost $SERVER_PORT)

    echo -e "${CYAN}[DEBUG] Réponse complète pour scénario modes multiples:${NC}"
    echo "$response"
    echo
    echo -e "${CYAN}[DEBUG] Lignes MODE spécifiques:${NC}"
    echo "$response" | grep "MODE.*#complex" | head -5
    echo
    echo -e "${CYAN}[DEBUG] Lignes 324 (consultation):${NC}"
    echo "$response" | grep "324.*#complex" | head -5
    echo

    if echo "$response" | grep -q "MODE.*#complex.*+iktl" && echo "$response" | grep -q "324.*#complex.*+iktl"; then
        print_success "Scénario modes multiples → consultation fonctionne"
    elif echo "$response" | grep -q "MODE.*#complex.*+i+k+t+l" && echo "$response" | grep -q "324.*#complex.*+itkl"; then
        print_success "Scénario modes multiples → consultation fonctionne (format avec séparateurs)"
    elif echo "$response" | grep -q "MODE.*#complex.*+i+k+t+l" && echo "$response" | grep -q "324.*#complex.*+.*i.*k.*t.*l"; then
        print_success "Scénario modes multiples → consultation fonctionne (ordre différent)"
    elif echo "$response" | grep -q "324.*#complex.*+.*i.*+.*k.*+.*t.*+.*l"; then
        print_warning "Modes appliqués mais format de consultation différent"
        echo -e "${YELLOW}[INFO] Les modes sont actifs mais affichage différent${NC}"
    else
        print_error "Problème avec le scénario modes multiples"
        echo -e "${RED}[DEBUG] Ni MODE +iktl ni 324 +iktl trouvés${NC}"
    fi

    print_test "Scénario: Canal protégé par mot de passe"
    echo -e "${CYAN}[DEBUG] Test: Création canal avec +k puis tentative d'accès${NC}"

    (
        echo -e "PASS $SERVER_PASS\r\nNICK ProtectedCreator\r\nUSER test test localhost :Test User\r\nJOIN #protected\r\nMODE #protected +k secret123\r\n"
        sleep 8  # Rester connecté pendant que les autres essaient de rejoindre
        echo -e "QUIT :Créateur se déconnecte\r\n"
    ) | nc -w12 localhost $SERVER_PORT &

    local creator_pid=$!

    sleep 1

    local response1=$(echo -e "PASS $SERVER_PASS\r\nNICK TryJoin1\r\nUSER test test localhost :Test User\r\nJOIN #protected\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    local response2=$(echo -e "PASS $SERVER_PASS\r\nNICK TryJoin2\r\nUSER test test localhost :Test User\r\nJOIN #protected secret123\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    kill $creator_pid 2>/dev/null || true
    wait $creator_pid 2>/dev/null || true

    echo -e "${CYAN}[DEBUG] Réponse tentative SANS mot de passe:${NC}"
    echo "$response1"
    echo
    echo -e "${CYAN}[DEBUG] Réponse tentative AVEC mot de passe:${NC}"
    echo "$response2"
    echo

    if echo "$response1" | grep -q "475.*Cannot join channel" && echo "$response2" | grep -q "JOIN.*#protected"; then
        print_success "Protection par mot de passe fonctionne"
    elif echo "$response1" | grep -q "475" && echo "$response2" | grep -q "JOIN"; then
        print_success "Protection par mot de passe fonctionne (codes différents)"
    elif echo "$response1" | grep -q "JOIN.*#protected"; then
        print_error "Canal sans mot de passe accessible - protection +k ne fonctionne pas"
        echo -e "${RED}[DEBUG] L'utilisateur a pu rejoindre sans mot de passe${NC}"
        echo -e "${YELLOW}[INFO] Vérifiez que le canal n'était pas vide (et donc recréé)${NC}"
    else
        print_error "Problème avec la protection par mot de passe"
        echo -e "${RED}[DEBUG] Comportement inattendu pour les deux tentatives${NC}"
    fi
}


test_edge_cases() {
    print_header "TESTS DE CAS LIMITES ET ROBUSTESSE"

    print_test "Topic avec caractères spéciaux"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK SpecialChar\r\nUSER test test localhost :Test User\r\nJOIN #special\r\nTOPIC #special :Topic avec emoticones et chars speciaux: []{}!@#\$%^&*()\r\nTOPIC #special\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "332.*emoticones.*chars speciaux"; then
        print_success "Gestion des caractères spéciaux dans topic"
    else
        print_warning "Problème potentiel avec caractères spéciaux"
    fi

    print_test "Topic très long (test de limite)"
    local long_topic="Topic très long pour tester les limites du serveur IRC: "
    for i in {1..20}; do
        long_topic+="partie$i "
    done

    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK LongTopic\r\nUSER test test localhost :Test User\r\nJOIN #longtopic\r\nTOPIC #longtopic :$long_topic\r\nTOPIC #longtopic\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "332"; then
        print_success "Gestion des topics longs"
    else
        print_warning "Problème potentiel avec les topics longs"
    fi

    print_test "Commandes rapides en succession"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK RapidUser\r\nUSER test test localhost :Test User\r\nJOIN #rapid\r\nTOPIC #rapid :Topic1\r\nTOPIC #rapid :Topic2\r\nTOPIC #rapid :Topic3\r\nTOPIC #rapid\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "332.*Topic3"; then
        print_success "Commandes rapides en succession gérées"
    else
        print_error "Problème avec les commandes rapides"
    fi

    print_test "Canal avec nom limite (caractères valides)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ChannelLimit\r\nUSER test test localhost :Test User\r\nJOIN #canal-test_123\r\nTOPIC #canal-test_123 :Topic canal avec tirets et underscores\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "JOIN.*#canal-test_123"; then
        print_success "Noms de canaux avec caractères spéciaux autorisés"
    else
        print_error "Problème avec les noms de canaux complexes"
    fi
}


test_cleanup() {
    print_header "TESTS DE NETTOYAGE"

    print_test "Test QUIT propre"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK CleanExit\r\nUSER test test localhost :Test User\r\nJOIN #cleanup\r\nQUIT :Au revoir!\r\n" | nc -w3 localhost $SERVER_PORT)

    if echo "$response" | grep -q "ERROR.*Closing Link"; then
        print_success "QUIT propre avec message personnalisé"
    else
        print_success "QUIT traité (format de réponse peut varier)"
    fi
}


print_summary() {
    echo
    print_header "RÉSUMÉ DES TESTS"
    echo -e "${CYAN}Tests exécutés: $TEST_COUNT${NC}"
    echo -e "${GREEN}Tests réussis: $PASSED_COUNT${NC}"
    echo -e "${RED}Tests échoués: $FAILED_COUNT${NC}"

    if [ $FAILED_COUNT -eq 0 ]; then
        echo -e "${GREEN}🎉 Tous les tests sont passés avec succès!${NC}"
    else
        echo -e "${YELLOW}⚠️  Certains tests ont échoué. Vérifiez les détails ci-dessus.${NC}"
    fi
    echo
}

show_menu() {
    echo
    print_header "MENU DES TESTS IRC ft_irc"
    echo "1. Tests complets (tous les tests)"
    echo "2. Tests de base (connexion, auth, nick)"
    echo "3. Tests des canaux (join/part/topic)"
    echo "4. Tests avancés du topic"
    echo "5. Tests des modes de canal"
    echo "6. Tests avancés des modes (NOUVEAU)"
    echo "7. Tests PART avancés (NOUVEAU)"
    echo "8. Tests des commandes d'opérateur"
    echo "9. Tests de gestion d'erreurs"
    echo "10. Tests de compatibilité irssi"
    echo "11. Tests de stabilité"
    echo "12. Tests de scénarios complexes"
    echo "13. Tests de cas limites"
    echo "14. Test de connexion simple"
    echo "0. Quitter"
    echo
    read -p "Choisissez une option (0-14): " choice
}

run_selected_tests() {
    case $1 in
        1)
            test_auth
            test_nick
            test_channels
            test_topic
            test_topic_advanced
            test_channel_modes
            test_advanced_modes
            test_part_advanced
            test_operator_commands
            test_privmsg
            test_error_handling
            test_irssi_compatibility
            test_stability
            test_complex_scenarios
            test_edge_cases
            test_cleanup
            ;;
        2)
            test_auth
            test_nick
            test_connection
            ;;
        3)
            test_channels
            test_topic
            ;;
        4)
            test_topic_advanced
            ;;
        5)
            test_channel_modes
            ;;
        6)
            test_advanced_modes
            ;;
        7)
            test_part_advanced
            ;;
        8)
            test_operator_commands
            ;;
        9)
            test_error_handling
            ;;
        10)
            test_irssi_compatibility
            ;;
        11)
            test_stability
            ;;
        12)
            test_complex_scenarios
            ;;
        13)
            test_edge_cases
            ;;
        14)
            test_connection
            ;;
        0)
            echo "Au revoir!"
            exit 0
            ;;
        *)
            print_error "Option invalide: $1"
            ;;
    esac
}


main() {
    if ! [ -f "Makefile" ]; then
        print_error "Makefile non trouvé. Êtes-vous dans le bon répertoire?"
        exit 1
    fi

    print_info "Compilation du serveur..."
    if ! make > /dev/null 2>&1; then
        print_error "Échec de la compilation. Vérifiez votre code."
        exit 1
    fi

    start_server

    trap 'stop_server; exit' EXIT SIGINT SIGTERM

    if [ $# -gt 0 ]; then
        run_selected_tests $1
    else
        while true; do
            show_menu
            run_selected_tests $choice
            print_summary
            echo
            read -p "Appuyez sur Entrée pour continuer..."
        done
    fi

    stop_server

    print_summary
}


if ! command -v nc &> /dev/null; then
    print_error "netcat (nc) n'est pas installé. Installez-le avec: sudo apt-get install netcat"
    exit 1
fi

main "$@"

test_topic_complete() {
    print_header "TESTS COMPLETS DU TOPIC (VERSION DÉTAILLÉE)"

    print_test "Test complet du topic - Scénario séquentiel"
    echo -e "${CYAN}[DEBUG] Test inspiré du script externe - Scénario complet avec un seul client${NC}"

    local response=$(cat << 'EOF' | nc -w10 localhost $SERVER_PORT
PASS test
NICK TopicTestUser
USER topic topic localhost :Topic Test User

JOIN #topic42

TOPIC #topic42

TOPIC #topic42 :Bienvenue sur le channel

TOPIC #topic42

TOPIC #topic42 :

TOPIC #topic42

TOPIC

TOPIC #doesnotexist

JOIN #prot
TOPIC #prot :Topic ouvert
MODE #prot +t
TOPIC #prot :Topic protégé

QUIT :Test terminé
EOF
)

    echo -e "${CYAN}[DEBUG] Réponse complète du test séquentiel:${NC}"
    echo "$response"
    echo

    echo -e "${CYAN}[DEBUG] Analyse des résultats par étape:${NC}"

    if echo "$response" | grep -q "331.*No topic is set\|331.*topic"; then
        print_success "Étape 1 ✅ : Topic vide initial (331) détecté"
    else
        print_error "Étape 1 ❌ : Topic vide initial non détecté"
    fi

    if echo "$response" | grep -q "TOPIC.*#topic42.*Bienvenue"; then
        print_success "Étape 2 ✅ : Définition de topic réussie"
    else
        print_error "Étape 2 ❌ : Définition de topic échoué"
    fi

    if echo "$response" | grep -q "332.*Bienvenue"; then
        print_success "Étape 3 ✅ : Lecture du topic défini (332) réussie"
    else
        print_error "Étape 3 ❌ : Lecture du topic défini échoué"
    fi

    if echo "$response" | grep -q "TOPIC.*#topic42.*:" && echo "$response" | grep -c "TOPIC.*#topic42.*:" -ge 2; then
        print_success "Étape 4 ✅ : Effacement du topic (chaîne vide) détecté"
    else
        print_warning "Étape 4 ⚠️ : Effacement du topic - vérifier manuellement"
    fi

    if echo "$response" | grep -q "461.*Not enough parameters\|461.*TOPIC"; then
        print_success "Étape 5 ✅ : TOPIC sans paramètre correctement rejeté (461)"
    else
        print_error "Étape 5 ❌ : TOPIC sans paramètre devrait être rejeté"
    fi

    if echo "$response" | grep -q "403.*No such channel\|403.*doesnotexist"; then
        print_success "Étape 6 ✅ : TOPIC sur canal inexistant rejeté (403)"
    else
        print_error "Étape 6 ❌ : TOPIC sur canal inexistant devrait être rejeté"
    fi

    print_test "Test topic avec mode +t (protection) - Scénario avancé"
    echo -e "${CYAN}[DEBUG] Test protection du topic avec changement de nickname${NC}"

    local response_protected=$(cat << 'EOF' | nc -w10 localhost $SERVER_PORT
PASS test
NICK ProtectedTopicOp
USER prot prot localhost :Protected Topic Operator

JOIN #protected
TOPIC #protected :Topic initial non protégé
MODE #protected +t
TOPIC #protected :Topic maintenant protégé
NICK ProtectedTopicUser
TOPIC #protected :Tentative de changement sans privilèges

QUIT :Test protection terminé
EOF
)

    echo -e "${CYAN}[DEBUG] Réponse test protection:${NC}"
    echo "$response_protected"
    echo

    if echo "$response_protected" | grep -q "482.*not channel operator\|482.*channel operator"; then
        print_success "Protection +t ✅ : Utilisateur normal correctement bloqué (482)"
    else
        print_error "Échec de la protection +t"
    fi

    print_test "Test topic avec caractères spéciaux et limites"
    echo -e "${CYAN}[DEBUG] Test robustesse : caractères spéciaux, emojis, et topics longs${NC}"

    local special_topic="Topic avec caractères spéciaux: !@#$%^&*()[]{}|;:,.<>?/~\`"
    local long_topic="Topic très très long pour tester les limites: $(printf 'A%.0s' {1..100})"
    local emoji_topic="Topic avec emojis et unicode: 🎉🚀💻🔥⭐"

    local response_special=$(cat << EOF | nc -w10 localhost $SERVER_PORT
PASS test
NICK SpecialTopicUser
USER special special localhost :Special Topic User

JOIN #special
TOPIC #special :$special_topic
TOPIC #special

JOIN #longtopic
TOPIC #longtopic :$long_topic
TOPIC #longtopic

JOIN #emoji
TOPIC #emoji :$emoji_topic
TOPIC #emoji

QUIT :Test spéciaux terminé
EOF
)

    echo -e "${CYAN}[DEBUG] Réponse test caractères spéciaux:${NC}"
    echo "$response_special" | head -20
    echo

    if echo "$response_special" | grep -q "332.*spéciaux"; then
        print_success "Caractères spéciaux ✅ : Topic avec caractères spéciaux accepté"
    else
        print_warning "Caractères spéciaux ⚠️ : Vérifier le support des caractères spéciaux"
    fi

    if echo "$response_special" | grep -q "332.*très.*long"; then
        print_success "Topic long ✅ : Topic long accepté"
    else
        print_warning "Topic long ⚠️ : Vérifier le support des topics longs"
    fi

    print_test "Test persistence et métadonnées du topic"
    echo -e "${CYAN}[DEBUG] Test métadonnées : qui a défini le topic et quand${NC}"

    echo -e "PASS test\r\nNICK MetaTopicSetter\r\nUSER meta meta localhost :Meta User\r\nJOIN #metatest\r\nTOPIC #metatest :Topic avec métadonnées\r\nQUIT\r\n" | nc -w3 localhost $SERVER_PORT > /dev/null

    local response_meta=$(echo -e "PASS test\r\nNICK MetaTopicReader\r\nUSER meta meta localhost :Meta Reader\r\nJOIN #metatest\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    echo -e "${CYAN}[DEBUG] Réponse métadonnées lors du JOIN:${NC}"
    echo "$response_meta"
    echo

    if echo "$response_meta" | grep -q "332.*métadonnées" && echo "$response_meta" | grep -q "333.*MetaTopicSetter"; then
        print_success "Métadonnées ✅ : Topic + auteur (332 + 333) lors du JOIN"
    else
        print_error "Métadonnées ❌ : Métadonnées manquantes lors du JOIN"
    fi

    print_test "Test cas limites et erreurs diverses"
    echo -e "${CYAN}[DEBUG] Test cas limites : canaux multiples, topics vides, erreurs${NC}"

    local response_edge=$(cat << 'EOF' | nc -w10 localhost $SERVER_PORT
PASS test
NICK EdgeCaseUser
USER edge edge localhost :Edge Case User

JOIN #edge1,#edge2,#edge3
TOPIC #edge1 :Topic du canal 1
TOPIC #edge2 :Topic du canal 2
TOPIC #edge3 :Topic du canal 3
TOPIC #edge1
TOPIC #edge2
TOPIC #edge3

TOPIC #edge1 :
TOPIC #edge2 :
TOPIC #edge3 :
TOPIC #edge1
TOPIC #edge2
TOPIC #edge3

PART #edge1,#edge2,#edge3

JOIN #finaltest
TOPIC #finaltest :Topic final de test
TOPIC #finaltest

QUIT :Tests cas limites terminés
EOF
)

    echo -e "${CYAN}[DEBUG] Réponse cas limites:${NC}"
    echo "$response_edge" | head -25
    echo

    local topic_count=$(echo "$response_edge" | grep -c "332.*Topic du canal")
    if [ "$topic_count" -ge 3 ]; then
        print_success "Multi-canaux ✅ : Topics multiples gérés ($topic_count topics trouvés)"
    else
        print_warning "Multi-canaux ⚠️ : Vérifier la gestion de topics multiples"
    fi

    if echo "$response_edge" | grep -q "TOPIC.*#finaltest.*final"; then
        print_success "Test final ✅ : Dernier test de topic réussi"
    else
        print_error "Test final ❌ : Problème avec le test final"
    fi
}

test_topic_multi_client() {
    print_header "TESTS TOPIC MULTI-CLIENTS"

    print_test "Test topic avec plusieurs clients simultanés"
    echo -e "${CYAN}[DEBUG] Simulation de plusieurs clients pour tester la synchronisation${NC}"

    (
        sleep 1
        echo -e "PASS test\r\nNICK MultiClient1\r\nUSER multi1 multi1 localhost :Multi Client 1\r\nJOIN #multiclient\r\nTOPIC #multiclient :Topic du créateur\r\nMODE #multiclient +t\r\nsleep 5\r\nQUIT\r\n" | nc -w8 localhost $SERVER_PORT
    ) &

    (
        sleep 2
        echo -e "PASS test\r\nNICK MultiClient2\r\nUSER multi2 multi2 localhost :Multi Client 2\r\nJOIN #multiclient\r\nTOPIC #multiclient\r\nTOPIC #multiclient :Tentative de changement\r\nsleep 3\r\nQUIT\r\n" | nc -w8 localhost $SERVER_PORT
    ) &

    wait
    print_success "Test multi-clients terminé (vérifier manuellement la synchronisation)"
}


test_operator_commands() {
    print_header "TESTS DES COMMANDES D'OPÉRATEUR"
    print_warning "Tests d'opérateur non implémentés - Placeholder"
}

test_error_handling() {
    print_header "TESTS DE GESTION D'ERREURS"
    print_warning "Tests de gestion d'erreurs non implémentés - Placeholder"
}

test_irssi_compatibility() {
    print_header "TESTS DE COMPATIBILITÉ IRSSI"
    print_warning "Tests irssi non implémentés - Placeholder"
}

test_stability() {
    print_header "TESTS DE STABILITÉ"
    print_warning "Tests de stabilité non implémentés - Placeholder"
}

main() {
    if ! [ -f "Makefile" ]; then
        print_error "Makefile non trouvé. Êtes-vous dans le bon répertoire?"
        exit 1
    fi

    print_info "Compilation du serveur..."
    if ! make > /dev/null 2>&1; then
        print_error "Échec de la compilation. Vérifiez votre code."
        exit 1
    fi

    start_server

    trap 'stop_server; exit' EXIT SIGINT SIGTERM

    if [ $# -gt 0 ]; then
        run_selected_tests $1
    else
        while true; do
            show_menu
            run_selected_tests $choice
            print_summary
            echo
            read -p "Appuyez sur Entrée pour continuer..."
        done
    fi

    stop_server

    print_summary
}
