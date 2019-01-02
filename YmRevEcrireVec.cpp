//---------------------------------------------------------------------------------------------------------
// Classe de lecture des donnees RESAVEN
// --------------------------------------------------------------------------------------------------------

using namespace std;

#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <YmRevGlobal.h>
#include <YmRevTcn.h>
#include <YmRevPassagersParcours.h>
#include <YmRevSegments.h>
#include <YmRevAnnulation.h>
#include <YmRevRetraits.h>
#include <YmRevEcrireVec.h>
#include <YmRevMess.h>
#include <YmRevTarifs.h>
#include <YmRevClassMapping.h>
#include <YmRevDeviceType.h>
#include <YmRevMapTrLeg.h>



#define LINESZ 512

YmRevEcrireVec::YmRevEcrireVec( char* NomFichier)
{
  strcpy (_NomFichier, NomFichier);

  /* Recherche du nom du fichier sans le chemin */

  int i =0;
  int j=0;

  for (i=strlen(_NomFichier)-1;i>0;i--) {
     if (_NomFichier[i] == '/') break;
  }

  _NomFichier_court[0]='\0'; 
   for (j=0;j<strlen(_NomFichier)-1-i;j++) {
     _NomFichier_court[j] = _NomFichier[i+1+j] ;
  }
  _NomFichier_court[j+1]='\0';

  /**/
 
}

void YmRevEcrireVec::Start()
{
    g_EcrireVecEnCours = true;
    lireFichierResaven();
    g_EcrireVecEnCours = false;
    cout<<endl;
    cout <<DonneHeure()<<" Fin Ecrire Vecteurs"<<endl;
    return;
}


int YmRevEcrireVec::lireFichierResaven()
{

    int num_enr=0; // valeur numerique du code enregistrement de la ligne lue  
    char szMess[70];
    // variables utilisees pour la lecture du fichier d'entree
    FILE *inf;
    char sLigne[LINESZ];

    // Initialisation des membres dedies a la lecture du ficher Resaven
    this->InitialiserMembresPourLectureFichierResaven();

    // Lecture du fichier d'entree
    inf = fopen(_NomFichier,"r");
    if (inf == NULL)  {
        cout << "Erreur a l'ouverture du fichier "<<_NomFichier<<endl;
        g_InfoBatch.setAssocData ("STATUS", "FINI");
        exit(10);
    }
    _iNumeroLigneFichier=0; 
    // Positionnement sur la ligne dont le code enregistrement est egal a 01 : ligne d'entete du fichier
	while ((!feof(inf)) && (num_enr !=1)) {
        fgets(sLigne,LINESZ,inf);
        _iNumeroLigneFichier++;
        sLigne[strlen(sLigne)-1]='\0';
        sscanf (&sLigne [0], "%2d", &num_enr);
    }
//
    if (feof(inf)) {
        cout << "fichier Resaven vide intitule "<<_NomFichier<<endl;
        sprintf( szMess,"fichier Resaven vide intitule %s",_NomFichier);
        g_InfoBatch.setAssocData ("STATUS", "FINI");
        sql_error(szMess,1);
        exit(10);
    }

//
    this->GestionDatesIssuesFichierResaven(sLigne);
    this->GestionLignesFichierResaven(inf);
    if (g_TraceEcrireVec) this->AffichageListesConstituees();;
    return 0;
}

void YmRevEcrireVec::GestionDatesIssuesFichierResaven(char * sLigne)
{
    char lsdate[8];
    char lsheure[5];
    char lsCtrlDate[13]; 
// controle de coherence de la date-heure de  entete du fichier Resaven
    sscanf (&sLigne [9], "%12s", lsCtrlDate);
    ctrl_date_entete_fichier(lsCtrlDate);
// memorisation des date et heure de reference (1ere ligne positions 9 a 16 pour la date, 17 a 20 pour l'heure)
    sscanf (&sLigne [9], "%8s", lsdate);
    sscanf (&sLigne [17], "%4s", lsheure);
	
// Mise a jour des variables globales g_szDateFichier (char[11]) et g_HeureFichier (int)
    int igda=0;
    for (int irda=0;irda<9;irda++) {
        if ((irda == 2) || (irda == 4)) {
            g_szDateFichier [igda]='/';
            g_szDateFichierShort[igda]='/';
            igda++;
        }
        g_szDateFichier [igda]=lsdate[irda];
        if (irda<4) g_szDateFichierShort[igda]=lsdate[irda];
        if (irda >5) g_szDateFichierShort[igda-2]=lsdate[irda];
        igda++;
    }
    g_szDateFichier[strlen(g_szDateFichier)]='\0';
    g_szDateFichierShort[strlen(g_szDateFichierShort)]='\0';
    g_HeureFichier=atoi(lsheure);

    // recuperation de la date heure du fichier precedent
    string sDateHeurePrec;
    sDateHeurePrec = g_InfoBatch.getAssocData ("LAST_RESAVEN");
    if (!sDateHeurePrec.empty())
    { 
        // On initailise avec la date heure du precedent.
       string sDateHeurePrec_Part = (sDateHeurePrec.substr(0,10)).c_str();
         
        YmDate DatePrec(sDateHeurePrec_Part) ;
        string sDateFichier = g_szDateFichier;
		cout << sDateFichier <<_NomFichier<<endl;
        YmDate DateFichier(sDateFichier);
        int HeurePrec = 100 * atoi (sDateHeurePrec.substr (11, 2).c_str()) + atoi (sDateHeurePrec.substr (14, 2).c_str());
        string sbuf;
        cout <<"Date/Heure Precedent : "<<DatePrec.PrintFrench(sbuf)<<" "<<HeurePrec<<"    rev_info_batch:"<<sDateHeurePrec<<endl;
        cout <<"Date/Heure Actuel    : "<<DateFichier.PrintFrench(sbuf)<<" "<<g_HeureFichier<<endl;
        if ((DatePrec.DaysFrom70() > DateFichier.DaysFrom70()) ||
            ((DatePrec.DaysFrom70() == DateFichier.DaysFrom70()) && (HeurePrec > g_HeureFichier)))
        {
            if (g_ForcerFichier)
               cout <<"Attention, traitement force sur fichiers RESAVEN non ordonnes"<<endl;
            else
            {
               cout <<"STOP : Fichiers resaven non ordonne"<<endl;
               g_InfoBatch.setAssocData ("STATUS", "FINI");
               exit (11);
            }
        }
        if ((DatePrec.DaysFrom70() == DateFichier.DaysFrom70()) && (HeurePrec == g_HeureFichier))
        {
            if (g_ForcerFichier)
                cout <<"Attention, re-traitement force sur le meme fichier RESAVEN"<<endl;
            else
            {
                cout <<"Resaven deja traite"<<endl;
                g_InfoBatch.setAssocData ("STATUS", "FINI");
                exit (12);
            }
        }
    }
    string sDateHeureFichier = g_szDateFichier;
    char szHeure[6];
    sprintf (szHeure, " %2.2u:%2.2u", g_HeureFichier / 100, g_HeureFichier % 100);
    sDateHeureFichier += szHeure;
    g_InfoBatch.setAssocData ("LAST_RESAVEN", sDateHeureFichier);
    // Memorisation du nom du fichier dans la table REV_INFO_BATCH (Key info LAST_RESAVEN_NAME)
    g_InfoBatch.setAssocData ("LAST_RESAVEN_NAME",_NomFichier);
    //
    string lstrdate_temp;
    lstrdate_temp=string(lsdate,8);
    _strDateEntete=lstrdate_temp.substr(4,4)+lstrdate_temp.substr(2,2)+lstrdate_temp.substr(0,2);
    _strHeureEntete=string(lsheure,4);
}
//JRO DT23805 YIELD IC SRO
bool YmRevEcrireVec::VerificationCodeEquipementResaven(string CodeEquipementResaven)
{
	
	return std::find(ListCodeEquipement.begin(), ListCodeEquipement.end(), CodeEquipementResaven) != ListCodeEquipement.end();
}

void YmRevEcrireVec::GestionLignesFichierResaven(FILE *inf)
{
    // variables utilisees pour la lecture du fichier d'entree
    char sLigne[LINESZ];
    memset (sLigne, '\0', LINESZ);
    int num_enr=0; // valeur numerique du code enregistrement de la ligne lue
    int num_struc=0; // valeur numerique du code sous-structure de la ligne lue
    int struc_prec=0; // valeur numerique du code sous-structure de la ligne precedemment acceptee
    int firstTcn=1; //  1 = indique que la ligne precedemment acceptee n'etait pas une ligne vente (code enregistrement <> 04), 0 sinon 
    int liSegmentNumEnCours=0; // numero de segment en cours.
    int liSegmentNum=0;
    int liNumLigne04=0;
    int liNumLigne00=0;
    int liNbTcnTraites=0;
    int liNbAnnulationsTraites=0;
    int liNbHermesTraites=0;
    int liNbRetraitsTraites=0;
    int liNbTcnRetenus=0;
    int liNbAnnulationsRetenus=0;
    int liNbHermesRetenus=0;
    int liNbRetraitsRetenus=0;
    int liStatut=0;
    char lsClassOfService[3];
	long lNumPalierSegment; // HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
    char lsTrancheCarrierCode[3];
	char codeEquipResaven[3]; //code equipement dans un segment du resaven //JRO DT23805 YIELD IC SRO
    // variables locales utilisees pour les controles des dates Max de depart des Tcn par rapport a la date de reference
    char lsdate[9];
    string  lstrdatetemp;
    string  lstrdateDepartSeg;

    // variables locales de stockage des lignes Ventes de structure 04 et 06
    char sLigne04[LINESZ];
    char sLigne06[LINESZ];

    // variable locale utilisee pour le controle d'existence dans la Map des TrNonYields
    char lsTrancheNo[7];

    //Message de debut de traitement du fichier RESAVEN
    YmRevMess rMess((char*)"batch_unitaire", (char*)"YmRevEcrireVec",(char*)"GestionLignesFichierResaven",(char*)"revenus");
    char sMessText[180];
    if (g_ForcerFichier)
        sprintf(sMessText,"Debut lecture RESAVEN Fichier : %s date : %s [options : /f]",(char*)_NomFichier_court,g_szDateFichierShort);
    else
        sprintf(sMessText,"Debut lecture RESAVEN Fichier : %s date : %s ",(char*)_NomFichier_court,g_szDateFichierShort);
    rMess.envoiMess(223,(char *)sMessText,YmRevMess::JOBSTART);

	lNumPalierSegment = -22;
    //DT33254
	Booking StructBooking;
    while (!feof(inf)) {
	
        sscanf (&sLigne [0], "%2d", &num_enr);
        switch (num_enr) {
            case 4:
				
				
                if (strlen(sLigne) < 36) {
                    sprintf(sMessText,"Fichier : %s (ligne %d), structure : %d longueur ligne %d trop courte pour connaitre sous_structure ou caractere parasite",_NomFichier_court,_iNumeroLigneFichier,num_enr,strlen(sLigne));
                    rMess.envoiMess(232,(char *)sMessText,YmRevMess::WARN);
                    break;
                }
				
                sscanf (&sLigne [34], "%2d", &num_struc);
                switch (num_struc) {
                    case 0:
						

                        if (!firstTcn) {
                            // Si le numero de structure de la ligne precedente est 02 ou 08 ou si c'est un auto train, annulation du segment precedent
                            if ((struc_prec == 2) || (struc_prec == 8) || ((struc_prec != 10 ) && _iAuto ))
								{
									this->AnnulationSegmentTemporairePrecedent(); 
								}
                            // Gestion de la fin du Tcn precedent
							// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
                            //this->GestionFinTcn(struc_prec,sLigne04,liNumLigne04,liNumLigne00);
							this->GestionFinTcn(struc_prec,sLigne04,liNumLigne04,liNumLigne00,lNumPalierSegment,StructBooking);
                        }
                        liNumLigne00=_iNumeroLigneFichier;
                        liNbTcnTraites++;
                        // Initialisation nouveau Tcn 
                        this->InitialisationNouveauTcn(sLigne);
                        // Reinitialisation de variables locales 
                        firstTcn=0;
                        struc_prec=0;
                        liSegmentNumEnCours=0;
                        lstrdateDepartSeg.clear();
						_isTGVorCORAIL=true;
						// TDR - Anomalie 90705
						_isTCNHasTGVorCORAIL=false;

                        break;

                    case 2:
						
						
                        if (!firstTcn) {
							
                            // Si segment precedent sans structure 08 on le retire du tableau temporaire des segments
							//AJOUT JRO DT23805 YIELD IC SRO Mise en commentaire de la ligne suivante
                            //if  (struc_prec == 2) this->AnnulationSegmentTemporairePrecedent();
							                                                

							
							
						/*
							

							if (VerificationCodeEquipementResaven(codeEquipResaven) == 0){
								//cout<<"Code Equipement TGV ou CORAIL? : " <<VerificationCodeEquipementResaven(codeEquipResaven)<<endl; 
								cout<<"_pSegments : "<<_pSegments<<endl;
								if (_pSegments != NULL){

									this->AnnulationSegmentTemporairePrecedent();
								}
								else cout<<"pSegment pas initialis� : "<<_pSegments<<endl;
								cout<<"done"<<endl;
								break;
							}
						
						*/
							
							
                            // Si le numero de structure de la ligne precedente est 04, generation d'instance de PassagerParcours
                            if (struc_prec == 4)  {


								
                                //if (_iAuto) this->AnnulationSegmentTemporairePrecedent(); JRO IC SRO DT 23805 : La seule mani�re de discriminer 
								// un train IC SRO d'une reservation auto train incompl�te est de v�rifier le code �quipement

								//Si le segment pr�cedent ne correspond pas � un TGV ou un CORAIL, c'est � dire � un AUTO TRAIN : //A VERIFIER, pas important 
								//pour le moment car dans tout les cas les trajets non CORAIL ou TGV sont rejet� dans gestionFinTCN
								if (!_isTGVorCORAIL) this->AnnulationSegmentTemporairePrecedent();

								// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
                                //this->TraiterPassagersParcoursSansInternational(sLigne04,liNumLigne04);
                                //DT33254 l'ajout du param�tre StructBooking
                                this->TraiterPassagersParcoursSansInternational(sLigne04,liNumLigne04,lNumPalierSegment, StructBooking);
                                // Reinitialisation du tableau temporaire des pointeurs segments
                                this->InitialiserTableauPointeursSegments();
                            }
                            if (strlen(sLigne) < 115) {
                                sprintf(sMessText,"Fichier : %s (ligne %d), structure : %d sous_structure : %d : longueur ligne incorrecte (%d) ou caractere parasite",_NomFichier_court,_iNumeroLigneFichier,num_enr,num_struc,strlen(sLigne));
                               rMess.envoiMess(273,(char *)sMessText,YmRevMess::WARN);
                               struc_prec=0;
                               break;
                            }

							//AJOUT JRO DT23805 YIELD IC SRO
							//Scan du code �quipement
							sscanf(&sLigne[112],"%3s",codeEquipResaven);
						//V�rification du typage TGV ou CORAIL , si pas TGV ou CORAIL, on rejete le TCN
							// TDR - Anomalie Dans le cas de train multi-segments il ne faut pas rejeter tout les segments d'un meme TCN
							_isTGVorCORAIL = /*_isTGVorCORAIL **/ VerificationCodeEquipementResaven(codeEquipResaven);
							
							if (_isTGVorCORAIL) 
							{
								// TDR - Anomalie 90705 Indique si le tcn a au moins un segment tgv ou corail
								_isTCNHasTGVorCORAIL = true;
							}
							//FIN AJOUT JRO DT23805 YIELD IC SRO
													
                            // generation d'une instance de YmRevSegments
							// HRI, 05/03/2015 : on recupere le palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
                            //this->InitialisationNouveauSegmentTemporaire(sLigne);
							
							this->InitialisationNouveauSegmentTemporaire(sLigne, lNumPalierSegment,&StructBooking);

                            // Affectation de variables locales
                            struc_prec=num_struc;
                            liSegmentNumEnCours=_pSegments->getSegmentNum();

                         }
                         break;

                     case 4:
						 
                        if (!firstTcn) {
                            if (strlen(sLigne) < 95) {
                                sprintf(sMessText,"Fichier : %s (ligne %d), structure : %d sous_structure : %d : longueur ligne incorrecte (%d) ou caractere parasite",_NomFichier_court,_iNumeroLigneFichier,num_enr,num_struc,strlen(sLigne));
                               rMess.envoiMess(293,(char *)sMessText,YmRevMess::WARN);
                               // reinitialisation du tableau temporaire de pointeurs segments
                                 this->InitialiserTableauPointeursSegments();
                                 liSegmentNumEnCours=0;
                                 lstrdateDepartSeg.clear();
                                 _iTotalDistanceSegmentsParcours=0;
                                 struc_prec=0;
                                 break;
                            }
                             // on abandonne la Ligne 04 si la 2eme position de la classe de Service = ' ' 
                             liNumLigne04=_iNumeroLigneFichier;
                             sscanf (&sLigne [42], "%2s", lsClassOfService);
                             // on abandonne la Ligne 04 si la classe de service est a blanc ou la 2eme position de la classe de Service = ' '
							 /*MISE EN COMMENTAIRE JRO DT23805 YIELD IC SRO 
							 //JRO DT23805 YIELD IC SRO  Quelle que soit la classe de service, sur 1 ou 2 caract�res, le TCN doit �tre pris en compte et trait�
                             if ((lsClassOfService[1] == ' ') || (strlen(lsClassOfService) == 1)) {
                                 // reinitialisation du tableau temporaire de pointeurs segments
                                 this->InitialiserTableauPointeursSegments();
                                 liSegmentNumEnCours=0;
                                 _iTotalDistanceSegmentsParcours=0;
                                 lstrdateDepartSeg.clear();
                                 struc_prec=0;
                                 break;
                             }
							 */

							  // si struc_prec == 2 on doit etre dans un cas 'Auto'
							 //JRO IC SRO - Mise en commentaire de la ligne suivante, car dans le cas d'un IC SRO, la struct 4 peut �tre preced�e d'un 02
                             //if (struc_prec == 2) _iAuto=1;
                             if ((struc_prec ==8) || (struc_prec==2))
                             {
                                 // Si la 1ere lettre de lsClassOfService n'existe pas dans la chaine des compartiments g_CmptList et si lsClassOfService n'existe pas dans la MapClassMapping abandon du PassagersParcours
                                 _iFlagAbsenceClassOfService=0;
                                 if (existenceCompart(lsClassOfService[0]) == NULL) {
                                 
//
                                     YmRevClassMapping* pClassMapping =g_MapClassMapping.isInMap(string(lsClassOfService,2));
                                     if (pClassMapping == NULL) {
                                         if (_iSegTemp  > 0) {
                                            // PLF : 18/06/2007 (suite)
                                            if (! _pSegTemp[_iSegTemp-1]->getDateDepart().empty())
                                                lstrdateDepartSeg=_pSegTemp[_iSegTemp-1]->getDateDepart().substr(6,2)+'/'+_pSegTemp[_iSegTemp-1]->getDateDepart().substr(4,2)+'/'+_pSegTemp[_iSegTemp-1]->getDateDepart().substr(2,2);
//
                                             sprintf(sMessText,"Fichier : %s (ligne %d), TCN %d : classe %s inconnue pour date circulation %s...PassagersParcours abandonne",_NomFichier_court,liNumLigne04,_pTcn->getTcnNumber(),lsClassOfService,lstrdateDepartSeg.c_str());
                                             rMess.envoiMess(341,(char *)sMessText,YmRevMess::WARN);
                                         }
                                         // reinitialisation du tableau temporaire de pointeurs segments
                                         this->InitialiserTableauPointeursSegments();
                                         liSegmentNumEnCours=0;
                                         _iTotalDistanceSegmentsParcours=0;
                                         lstrdateDepartSeg.clear();
                                         struc_prec=0;
                                         break;
                                     }
                                     else _iFlagAbsenceClassOfService=1;
                                 }
                                 strcpy(sLigne04,sLigne);
                                 struc_prec=num_struc;
                             }

                         }
                         break;
        
                     case 6:
						
                         if (!firstTcn) {
                             if (strlen(sLigne) < 105) {
                                 sprintf(sMessText,"Fichier : %s (ligne %d), structure : %d sous_structure : %d : longueur ligne incorrecte (%d) ou caractere parasite",_NomFichier_court,_iNumeroLigneFichier,num_enr,num_struc,strlen(sLigne));
                                 rMess.envoiMess(365,(char *)sMessText,YmRevMess::WARN);
                                 // reinitialisation du tableau temporaire de pointeurs segments
                                 this->InitialiserTableauPointeursSegments();
                                 liSegmentNumEnCours=0;
                                 _iTotalDistanceSegmentsParcours=0;
                                 lstrdateDepartSeg.clear();
                                 struc_prec=0;
                                 break;
                             }

                             if (struc_prec == 4) {
                                if (_iAuto) this->AnnulationSegmentTemporairePrecedent();
                                 strcpy(sLigne06,sLigne);

                                 // traitement du PassagersParcours associe aux lignes associees de structures 04 et 06
 								// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
                                 //this->TraiterPassagersParcoursAvecInternational(sLigne04,sLigne06,liNumLigne04);
                                 //DT33254 l'ajout du param�tre StructBooking
                                 this->TraiterPassagersParcoursAvecInternational(sLigne04,sLigne06,liNumLigne04,lNumPalierSegment, StructBooking);
                                 struc_prec=num_struc;
                             }
                             else struc_prec=0;

                             // reinitialisation du tableau temporaire de pointeurs segments 
                             this->InitialiserTableauPointeursSegments();
                             liSegmentNumEnCours=0;
                             _iTotalDistanceSegmentsParcours=0;
                             lstrdateDepartSeg.clear();
                         }                    
                         break;

                     case 8:
						
                         if (!firstTcn) {
                             if (strlen(sLigne) < 110) {
                                 sprintf(sMessText,"Fichier : %s (ligne %d), structure : %d sous_structure : %d : longueur ligne incorrecte (%d) ou caractere parasite",_NomFichier_court,_iNumeroLigneFichier,num_enr,num_struc,strlen(sLigne));
                                 rMess.envoiMess(397,(char *)sMessText,YmRevMess::WARN);
                                 break;
                             }

                             if  (struc_prec == 2) {
                                 // on verifie que le numero de segment de la ligne est egal a celui de la ligne de structure 02 precedente
                                 // sinon la ligne n'est pas retenue  
                                 sscanf (&sLigne [36], "%2d", &liSegmentNum);
                                 if (liSegmentNum != liSegmentNumEnCours) break;
                                 // on verifie que la date de paiement existe, sinon la ligne n'est pas retenue.
                                 sscanf (&sLigne [86], "%8s", lsdate);
                                 if ((!strcmp(lsdate,"00000000")) || (!strcmp(lsdate,"        ")) || (strlen(lsdate) != 8)) break;

                                 // si le code TrancheCarrierCode est different de 'SN' pour SNCF ou 'TH' pour Thalys
                                 // la ligne n'est pas retenue.
                                 sscanf (&sLigne [75], "%2s", lsTrancheCarrierCode);
                                 if (strcmp(lsTrancheCarrierCode,g_Client)) break;

                                 // si le couple ( no tranche,date de depart) est present dans la Map TrNonYield on ignore la ligne 
                                 sscanf (&sLigne [77], "%6s", lsTrancheNo);
       		                 lstrdatetemp=string(lsdate,8);
                                  string Part1 = lstrdatetemp.substr(4,4).c_str();
                                  string Part2 = lstrdatetemp.substr(2,2).c_str();
                                  string Part3 = lstrdatetemp.substr(2,2).c_str();
                                  string Part = (Part1 + Part2 + Part3).c_str();
                                  YmDate ldate_deb(Part);
                                 if (g_MapTrNonYield.isInMap(atol(lsTrancheNo),ldate_deb)) break;
                        
                                 // Mise a jour de l'instance du segment
                                 _pSegments->initSegments08(sLigne);
                                 if (! _pSegments->getDateDepart().empty())
                                     lstrdateDepartSeg=_pSegments->getDateDepart().substr(6,2)+'/'+_pSegments->getDateDepart().substr(4,2)+'/'+_pSegments->getDateDepart().substr(2,2);
                                 struc_prec=num_struc;
                                 //_iAuto=0; JRO IC SRO DT 23805 : Inutil
								 _PresenceSousStructure08=1;

                             }
                         } 
                         break;

                     case 10:
						
                         if (!firstTcn) 
						 {
                             if (strlen(sLigne) < 147) {
                                 sprintf(sMessText,"Fichier : %s (ligne %d), structure : %d sous_structure : %d : longueur ligne incorrecte (%d) ou caractere parasite",_NomFichier_court,_iNumeroLigneFichier,num_enr,num_struc,strlen(sLigne));
                                 rMess.envoiMess(436,(char *)sMessText,YmRevMess::WARN);
                                 break;
                             }
                             //if  (!_iAuto || (struc_prec != 4)) JRO IC SRO - DT 23805 - Reg train Auto : Test sur iAuto plus pertinent
							 //JRO IC SRO - DT 23805 - Reg train Auto : Avant IC SRO, tester iAuto �tait pertinent car 
							 // segment avec structure 08 ou sans structure 04 => on le retire de la liste temporaire des segments
							 if  (_PresenceSousStructure08 || (struc_prec != 4))
							 {
                                 
                                 this->AnnulationSegmentTemporairePrecedent();
                                 if (_iSegTemp >0)
                                      struc_prec=10;
                                 else struc_prec=0;
                             }
                             else
                             {
                                 // Mise a jour de l'instance du segment
                                 _pSegments->initSegments10(sLigne);
                                 if (! _pSegments->getDateDepart().empty())
                                     lstrdateDepartSeg=_pSegments->getDateDepart().substr(6,2)+'/'+
                                                   _pSegments->getDateDepart().substr(4,2)+'/'+_pSegments->getDateDepart().substr(2,2);
 								// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
                                // this->TraiterPassagersParcoursSansInternational(sLigne04,liNumLigne04);
                                //DT33254 l'ajout du param�tre StructBooking
								 this->TraiterPassagersParcoursSansInternational(sLigne04,liNumLigne04,lNumPalierSegment, StructBooking);
                                 // Reinitialisation du tableau temporaire des pointeurs segments
                                 this->InitialiserTableauPointeursSegments();
                                 struc_prec=num_struc;
                            }
                             //_iAuto=0; JRO IC SRO - DT 23805 - Reg train Auto : _iAuto est d�sormais toujours � O
							 _PresenceSousStructure08=0;
                         }

                         break;


                 default:
                         break;
                 }
                 break;
 

             case 7:
                if (!firstTcn) {
                    // Gestion de la Fin du Tcn precedent
					// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
                    //this->GestionFinTcn(struc_prec,sLigne04,liNumLigne04,liNumLigne00);
                    this->GestionFinTcn(struc_prec,sLigne04,liNumLigne04,liNumLigne00,lNumPalierSegment,StructBooking);
                    firstTcn=1;
                }
                  //PLF 30/10/2007: Les annulations sont desormais detaillees sur 3 lignes (sous-structures 00, 02, 04)
                  //JMG 12/12/2007: la sous structure 0 indique les montants, la sous structure 4 indique les code gares
                  sscanf (&sLigne [99], "%2d", &num_struc);
                  switch (num_struc) {
                    case 0:
                        if (strlen(sLigne) < 138) {
                            sprintf(sMessText,"Fichier : %s (ligne %d), structure : %d, sous structure : %d : longueur ligne incorrecte (%d) ou caractere parasite",_NomFichier_court,_iNumeroLigneFichier,num_enr,num_struc,strlen(sLigne));
                            rMess.envoiMess(448,(char *)sMessText,YmRevMess::WARN);
                            break;
                        }
                        liNbAnnulationsTraites++;
                        _pAnnulation = new YmRevAnnulation();
                        _pAnnulation->initAnnulation0700(sLigne);
                        _pAnnulation->alimenterVecteurAnnulation();
                        break;
                    case 4:
                        if (strlen(sLigne) < 112) {
                            sprintf(sMessText,"Fichier : %s (ligne %d), structure : %d, sous structure : %d : longueur ligne incorrecte (%d) ou caractere parasite",_NomFichier_court,_iNumeroLigneFichier,num_enr,num_struc,strlen(sLigne));
                            rMess.envoiMess(459,(char *)sMessText,YmRevMess::WARN);
                            break;
                         }
                        if (_pAnnulation)
                        { 
                          if (_pAnnulation->getStationOrigine().empty())
                          {
                            _pAnnulation->initAnnulation0704(sLigne);
                          }
                        }
                        break;
                    default:
                        break;

                  } // switch (num_struc)

                break;
 
            case 9:
                if (!firstTcn) {
                    // Gestion de la Fin du Tcn precedent
					// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
                    //this->GestionFinTcn(struc_prec,sLigne04,liNumLigne04,liNumLigne00);
					this->GestionFinTcn(struc_prec,sLigne04,liNumLigne04,liNumLigne00,lNumPalierSegment,StructBooking);
                    firstTcn=1;
                }
                if (strlen(sLigne) < 49) {
                    sprintf(sMessText,"Fichier : %s (ligne %d), structure : %d : longueur ligne incorrecte (%d) ou caractere parasite",_NomFichier_court,_iNumeroLigneFichier,num_enr,strlen(sLigne));
                    rMess.envoiMess(472,(char *)sMessText,YmRevMess::WARN);
                    break;
                }

                liNbHermesTraites++;
                _pAnnulation = new YmRevAnnulation();
                _pAnnulation->initAnnulation09(sLigne);
                _pAnnulation->alimenterVecteurAnnulation();
                break;

            case 12:
                if (!firstTcn) {
                    // Gestion de la Fin du Tcn precedent
					// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
                    //this->GestionFinTcn(struc_prec,sLigne04,liNumLigne04,liNumLigne00);
					this->GestionFinTcn(struc_prec,sLigne04,liNumLigne04,liNumLigne00,lNumPalierSegment,StructBooking);
					firstTcn=1;
                }

                if (strlen(sLigne) == 0) break;
                //DM6351 : statut d impression doit etre egal a 1 pour prise en compte du Retrait
                if (strlen(sLigne) <94) break;
                if ( sLigne [93] == ' ') break; 
                sscanf (&sLigne [93], "%1d", &liStatut);
                if (liStatut != 1) break;
                //
                liNbRetraitsTraites++;

                _pRetraits = new YmRevRetraits();
                _pRetraits->initRetraits(sLigne);
                // Si le canal emission n'est pas nul et n'est pas dans la Map DeviceType => emission message
                if (! _pRetraits->getCanalEmission().empty()) {
                    if (!g_MapDeviceType.isInMap(_pRetraits->getCanalEmission())) {
                        lstrdatetemp.clear();
                        if (! _pRetraits->getDateTransaction().empty()) 
                            lstrdatetemp=_pRetraits->getDateTransaction().substr(6,2)+'/'+_pRetraits->getDateTransaction().substr(4,2)+'/'+_pRetraits->getDateTransaction().substr(2,2);
                        sprintf(sMessText,"Fichier : %s (ligne %d), Retrait TCN no %d date transaction : %s : canal emission %s inconnu",_NomFichier_court,_iNumeroLigneFichier,_pRetraits->getTcnNumber(),lstrdatetemp.c_str(),_pRetraits->getCanalEmission().c_str());
                        rMess.envoiMess(500,(char *)sMessText,YmRevMess::WARN);
                    }
                }
                _pRetraits->alimenterVecteurRetraits();
                break;

            default:
                if (!firstTcn) {
                    // Gestion de la Fin du Tcn precedent
					// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
                    //this->GestionFinTcn(struc_prec,sLigne04,liNumLigne04,liNumLigne00);
                    this->GestionFinTcn(struc_prec,sLigne04,liNumLigne04,liNumLigne00,lNumPalierSegment,StructBooking);
                    firstTcn=1;
                }
                break;

        }
        fgets(sLigne,LINESZ,inf);
		 _iNumeroLigneFichier++;
        sLigne[strlen(sLigne)-1]='\0';
    }
    fclose(inf);
	// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
    //if (!firstTcn) this->GestionFinTcn(struc_prec,sLigne04,liNumLigne04,liNumLigne00);
    if (!firstTcn) this->GestionFinTcn(struc_prec,sLigne04,liNumLigne04,liNumLigne00,lNumPalierSegment,StructBooking);
    this->InitialiserTableauPointeursSegments();
    //Messages de type INF: tcn, annulations, Hermes, Retraits traites et retenus
    sprintf(sMessText,"Fichier : %s date : %s : Nb TCN traites : %d",(char*)_NomFichier_court,g_szDateFichier,liNbTcnTraites);
    rMess.envoiMess(524,(char *)sMessText,YmRevMess::INF);
    liNbTcnRetenus=g_VTcn.size();
    sprintf(sMessText,"Fichier : %s date : %s : Nb TCN retenus : %d",(char*)_NomFichier_court,g_szDateFichier,liNbTcnRetenus);
    rMess.envoiMess(527,(char *)sMessText,YmRevMess::INF);
    sprintf(sMessText,"Fichier : %s date : %s : Nb annulations traitees : %d",(char*)_NomFichier_court,g_szDateFichier,liNbAnnulationsTraites);
    rMess.envoiMess(529,(char *)sMessText,YmRevMess::INF);
    sprintf(sMessText,"Fichier : %s date : %s : Nb Hermes traites : %d",(char*)_NomFichier_court,g_szDateFichier,liNbHermesTraites);
    rMess.envoiMess(531,(char *)sMessText,YmRevMess::INF);
    sprintf(sMessText,"Fichier : %s date : %s : Nb Retraits traites : %d",(char*)_NomFichier_court,g_szDateFichier,liNbRetraitsTraites);
    rMess.envoiMess(533,(char *)sMessText,YmRevMess::INF);

    //Message de fin de lecture du fichier RESAVEN
    sprintf(sMessText,"Fin lecture RESAVEN Fichier : %s date : %s ",(char*)_NomFichier_court,g_szDateFichierShort);
    rMess.envoiMess(537,(char *)sMessText,YmRevMess::JOBEND);

}

// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
//void YmRevEcrireVec::TraiterPassagersParcoursSansInternational(char * sLigne04,int iNumLigne04)
//DT33254 l'ajout du param�tre StructBooking
void YmRevEcrireVec::TraiterPassagersParcoursSansInternational(char * sLigne04,int iNumLigne04,long lNumPalierSegment, Booking &StructBooking)
{
	
    char lsCodeTarif[5];
    char lsClassOfService[3];
    char lsTypePassager[7];
    int llMontantUnitTransport=0;
    double ldMontantTaxation=0;
    int liNestLevel=0;
    int liTypePassager=0;
    int liSegEff=0;
    string lstrDateDepart;
    YmRevMess rMess((char*)"batch_unitaire", (char*)"YmRevEcrireVec",(char*)"TraiterPassagersParcoursSans",(char*)"revenus");
    char sMessText[180];
    YmRevTarifs* pTarifs = NULL;

    string lstrdateDepartSeg;
    lstrdateDepartSeg.clear(); 

    sscanf (&sLigne04 [42], "%2s", lsClassOfService);


    sscanf (&sLigne04 [38], "%4s", lsCodeTarif);
    if ((!strcmp(lsCodeTarif,"0000")) || (!strcmp(lsCodeTarif,"    ")) || (strlen(lsCodeTarif) != 4)) { 
        liTypePassager=1;
    }
    else 
    {
        liTypePassager=0;
		// HRI, 06/03/2015 : le numero de palier fait partie de la recherche dans la map des codes tarifs
        pTarifs = g_MapTarifs.isInMap(string(lsCodeTarif,4),string(lsClassOfService,2), lNumPalierSegment,
                                      YmDate (_pTcn->getDatePaiement()));

    }

    // Controle du nombre de segments fiables a affecter a un passagerparcours => si aucun segment fiable pas de passager parcours
    liSegEff=0;
    for (int lis=0;lis<_iSegTemp;lis++) {
		//Controle de la date de depart dans pSegTemp
        YmDate ldate_depart(_pSegTemp[lis]->getDateDepart());
        if (! _pSegTemp[lis]->getDateDepart().empty())
            lstrdateDepartSeg=_pSegTemp[lis]->getDateDepart().substr(6,2)+'/'+_pSegTemp[lis]->getDateDepart().substr(4,2)+'/'+_pSegTemp[lis]->getDateDepart().substr(2,2);

        
//MHUE 11/09/06
        string szdate_depart_tr;
        ldate_depart.PrintFrench(szdate_depart_tr);
        TrLeg aTrLeg=g_MapTrLeg.InMap(_pSegTemp[lis]->getTrancheNo(),_pSegTemp[lis]->getOrigineSeg(),
                                      _pSegTemp[lis]->getDestinationSeg(), (char *)szdate_depart_tr.data());
        if (aTrLeg.indic_tgv!=2)
           {
           szdate_depart_tr = aTrLeg.dpt_date_tr;
           if (YmDate(szdate_depart_tr) < g_dateMinTranche )
               {
               _iTotalDistanceSegmentsParcours -= _pSegTemp[lis]->getLongueurSeg();
			  
               continue;
               }
           }
//MHUE 11/09/06

        // recherche du nest level du segment en fonction de son Origine, sa Destination,
        // sa classe de service, sa Date de depart, la date du jour et le numero de tranche
        // si la valeur du nestLevel n'est pas trouvee dans la ligne vente de sous_structure 02

		//JRO DT23805 IC SRO - Le test suivant n'est plus valable, on prend les trains qu'ils soient yield� ou non, cad qu'ils aient une classe de controle
		//ou non

		/*
        if (_pSegTemp[lis]->getNestLevel() == -1) 
        {

            // La valeur du nest level n'est pas trouvee, on ne prend pas le segment 
            _iTotalDistanceSegmentsParcours -= _pSegTemp[lis]->getLongueurSeg();
			
            continue;
        }
		*/

        liSegEff++;
    }

    if (liSegEff== 0) {
        _iTotalDistanceSegmentsParcours=0;
		 return;
    }

   if( _iFlagAbsenceClassOfService==1)
   {
       sprintf(sMessText,"Fichier : %s (ligne %d), TCN %d : 1er caractere de la classe %s non repertorie dans la chaine des compartiments, mais classe presente dans Map ClassMapping a la date depart %s",_NomFichier_court,iNumLigne04,_pTcn->getTcnNumber(),lsClassOfService,lstrdateDepartSeg.c_str());
       rMess.envoiMess(628,(char *)sMessText,YmRevMess::WARN);
       _iFlagAbsenceClassOfService=0;
	   
       return; 
   }
 

    for (int lipass=0;lipass<4;lipass++) {
        sscanf (&sLigne04 [44+67*lipass], "%6s", lsTypePassager);

        if ((!strcmp(lsTypePassager,"000000")) || (!strcmp(lsTypePassager,"      ")) || (strlen(lsTypePassager) != 6)) {
			
			break;	
		}

        sscanf (&sLigne04 [65+67*lipass], "%10d", &llMontantUnitTransport);

        YmRevTypePassager* pTypePassager=NULL;
		// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 et cette map sur les types de passagers
        if (liTypePassager==1) 
            pTypePassager=g_MapTypePassager.isInMap(string(lsTypePassager,6),string(lsClassOfService,2),lNumPalierSegment, YmDate (_pTcn->getDatePaiement()));
        
		
		_iNbPassagersParcours++;

        // generation d'une instance de YmRevPassagersParcours
        _pPassagersParcours = new YmRevPassagersParcours();
        _pPassagersParcours->initPassagersParcours(sLigne04,lipass);

        // Si la date de paiement est a null, on lui affecte la date de la premiere ligne du fichier
        if (_pPassagersParcours->getDatePaiement().empty()) _pPassagersParcours->setDatePaiement(_strDateEntete);

        // Controle du format  de la valeur du Code Tarif et du Type passager si aucun des 2 n'est trouve dans sa Map
        if ((pTarifs == NULL) && (pTypePassager == NULL)) {
            // Controle du format de Code Tarif
           if (! _pPassagersParcours->FormatCodeTarifOk()) {
               sprintf(sMessText,"Fichier : %s (ligne %d), TCN %d : format code tarif %s incorrect",_NomFichier_court,iNumLigne04,_pPassagersParcours->getTcnNumber(),_pPassagersParcours->getCodeTarif().c_str());
               rMess.envoiMess(659,(char *)sMessText,YmRevMess::WARN);
           }
           // Controle du format de Type Passager
           if (! _pPassagersParcours->FormatTypePassagerOk()) {
               sprintf(sMessText,"Fichier : %s (ligne %d), TCN %d : format type passager %s incorrect",_NomFichier_court,iNumLigne04,_pPassagersParcours->getTcnNumber(),_pPassagersParcours->getPassagerType().c_str());
               rMess.envoiMess(664,(char *)sMessText,YmRevMess::WARN);
           }
        }
                

        // Affectation du pointeur du Tcn courant
        _pPassagersParcours->setPTcnCourant(_pTcn);

        //DT33254 affectation des variables Booking
        _pPassagersParcours->setClasseBooking(StructBooking.classOfServiceBooking);
		_pPassagersParcours->setPalierBooking(StructBooking.numPalierBooking);
		_pPassagersParcours->setUpgHan(StructBooking.UpgHan);

        // Calcul du montant taxation
        ldMontantTaxation=_pPassagersParcours->calculMontantTaxation(pTarifs,pTypePassager,this->_iTotalDistanceSegmentsParcours,llMontantUnitTransport);

        // Traitement des Segments a rajouter au PassagersParcours
        this->TraiterSegments(ldMontantTaxation,liSegEff,pTypePassager,iNumLigne04);


        // rajout du PassagerParcours dans la liste des PassagersParcours du Tcn
        _pTcn->ajouterPassagersParcours(_pPassagersParcours);
    }
} 

// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
//void YmRevEcrireVec::TraiterPassagersParcoursAvecInternational(char * sLigne04,char * sLigne06,int iNumLigne04 )
//DT33254 l'ajout du param�tre StructBooking
void YmRevEcrireVec::TraiterPassagersParcoursAvecInternational(char * sLigne04,char * sLigne06,int iNumLigne04, long numPalierSegment, Booking StructBooking )
{
    char lsClassOfService[3];
    string lstrClassOfService;
	//DM 7978 - JLA - Ajout du Palier
    int lsNumPalier = -1;
    char lsTypePassager[7];
    int llMontantDomestique=0;
    int llMontantEtranger=0;
    double ldMontantTaxation=0;
    int liNestLevel=0;
    int liSegEff=0;
    YmRevMess rMess((char*)"batch_unitaire", (char*)"YmRevEcrireVec",(char*)"TraiterPassagersParcoursAvec",(char*)"revenus");
    char sMessText[180];
	char lsnumber[3];

    sscanf (&sLigne04 [42], "%2s", lsClassOfService);
    // recherche s'il existe un type de passager commencant par 'ET'
    // Si oui conversion de sa classe de service par l'intermediaire de la Map ClassMapping relative a la table SC_CLASS_MAPPING 
    for (int lipass=0;lipass<4;lipass++) {

        sscanf (&sLigne06 [76+29*lipass], "%6s", lsTypePassager);
        if ((!strcmp(lsTypePassager,"000000")) || (!strcmp(lsTypePassager,"      ")) || (strlen(lsTypePassager) != 6)) break;
        if (string(lsTypePassager,2)== "ET") 
        {
            // Dans la Map ClassMapping, le c equivaut au GroupeDevice du TypeDevice recupere dans la ligne 00
            YmRevClassMapping* pClassMapping =g_MapClassMapping.isInMap(string(lsClassOfService,2));
            if (pClassMapping != NULL) {
                lstrClassOfService=pClassMapping->get_ClassDest();
                strncpy(lsClassOfService,lstrClassOfService.c_str(),2);
            }
            break;
        }
    }

    // Controle du nombre de segments fiables a affecter a un passagerparcours => si aucun segment fiable pas de passager parcours
    liSegEff=0;
    for (int lis=0;lis<_iSegTemp;lis++) {
        YmDate ldate_depart(_pSegTemp[lis]->getDateDepart());
//MHUE 11/09/06
        string szdate_depart_tr;
        ldate_depart.PrintFrench(szdate_depart_tr);
        TrLeg aTrLeg=g_MapTrLeg.InMap(_pSegTemp[lis]->getTrancheNo(),_pSegTemp[lis]->getOrigineSeg(),
                                          _pSegTemp[lis]->getDestinationSeg(), (char *)szdate_depart_tr.data());
        if (aTrLeg.indic_tgv!=2)
           {
           szdate_depart_tr = aTrLeg.dpt_date_tr;
           if (YmDate(szdate_depart_tr) < g_dateMinTranche )
               {
               _iTotalDistanceSegmentsParcours -= _pSegTemp[lis]->getLongueurSeg();
               continue;
               }
           }
//MHUE 11/09/06

        // recherche du nest level du segment en fonction de son Origine, sa Destination,
        // sa classe de service, sa Date de depart, la date du jour et le numero de tranche
        if (_pSegTemp[lis]->getNestLevel() == -1)
        {
            // La valeur du nest level n'est pas trouvee, on ne prend pas le segment
            if ( string(lsTypePassager,2)== "ET") {
                    sprintf(sMessText,"Fichier : %s (ligne %d), TCN %d : type de passager ET non retenu pour un segment international " ,_NomFichier_court,iNumLigne04,_pTcn->getTcnNumber());
                    rMess.envoiMess(780,(char *)sMessText,YmRevMess::WARN);
            }

            _iTotalDistanceSegmentsParcours -= _pSegTemp[lis]->getLongueurSeg();
            continue;

        }
        liSegEff++;
		//DM7978 - JLA - Prise compte du palier
		lsNumPalier = _pSegTemp[lis]->getNumPalier();

    }

    if (liSegEff== 0) {
        _iTotalDistanceSegmentsParcours=0;
        return;
    }

    for (int lipass=0;lipass<4;lipass++) {
 
        sscanf (&sLigne06 [76+29*lipass], "%6s", lsTypePassager);
        if ((!strcmp(lsTypePassager,"000000")) || (!strcmp(lsTypePassager,"      ")) || (strlen(lsTypePassager) != 6)) break;

        sscanf (&sLigne06 [85+29*lipass], "%10d", &llMontantDomestique);
        sscanf (&sLigne06 [95+29*lipass], "%10d", &llMontantEtranger);
        llMontantDomestique+=llMontantEtranger; 

        YmRevTypePassager* pTypePassager=NULL;
		// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 et cette map sur les types de passagers
        pTypePassager=g_MapTypePassager.isInMap(string(lsTypePassager,6),string(lsClassOfService,2),numPalierSegment,
                                                    YmDate (_pTcn->getDatePaiement()));
		//cout   << " pTypePassager=g_MapTypePassager.isInMap : " << (pTypePassager != NULL) << endl;
        _iNbPassagersParcours++;

        // generation d'une instance de YmRevPassagersParcours
        _pPassagersParcours = new YmRevPassagersParcours();
        _pPassagersParcours->initPassagersParcours(sLigne04,lipass);
        _pPassagersParcours->initPassagersParcours(sLigne06,lipass);
        
        // Maj de la ClassOfService de l'instance initialisee avec la valeur 'string(lsClassOfService,2)'
         string lsCLassOfService_part = string(lsClassOfService,2);
        _pPassagersParcours->setClassOfService(lsCLassOfService_part);
       
        // Si la date de paiement est a null, on lui affecte la date de la premiere ligne du fichier
        if (_pPassagersParcours->getDatePaiement().empty()) _pPassagersParcours->setDatePaiement(_strDateEntete);

        // Controle du format de Type Passager
        if (! _pPassagersParcours->FormatTypePassagerOk()) {
            sprintf(sMessText,"Fichier : %s (ligne %d), TCN %d : format type passager % incorrect",(char *)_NomFichier_court,iNumLigne04,_pPassagersParcours->getTcnNumber(),_pPassagersParcours->getPassagerType().c_str());
            rMess.envoiMess(801,(char *)sMessText,YmRevMess::WARN);
        }
 
       
        // Affectation du pointeur du Tcn courant
        _pPassagersParcours->setPTcnCourant(_pTcn);
		
		// Booking 
        _pPassagersParcours->setClasseBooking(StructBooking.classOfServiceBooking);
		_pPassagersParcours->setPalierBooking(StructBooking.numPalierBooking);
		_pPassagersParcours->setUpgHan(StructBooking.UpgHan);
		
        // Calcul du montant taxation
        ldMontantTaxation=_pPassagersParcours->calculMontantTaxation(NULL,pTypePassager,this->_iTotalDistanceSegmentsParcours,llMontantDomestique);

        // Traitement des Segments a rajouter au PassagersParcours
        this->TraiterSegments(ldMontantTaxation,liSegEff,pTypePassager,iNumLigne04);

        // rajout du PassagerParcours dans la liste des PassagrsParcours du Tcn
        _pTcn->ajouterPassagersParcours(_pPassagersParcours);
    }
}

void YmRevEcrireVec::TraiterSegments(double ldMontantTaxation,int liSegEff, YmRevTypePassager * pTypePassager,int iNumLigne04)
{
    int liNestLevel=0;
    string lstrDateDepart;
    YmRevMess rMess((char*)"batch_unitaire", (char*)"YmRevEcrireVec",(char*)"TraiterSegments",(char*)"revenus");
    char sMessText[180];
 
    for (int lis=0;lis<_iSegTemp;lis++) {
        // D�but Correction Ano - 98698 : Ignorer tous les segments dont le code �quipement n'appartient pas � la liste des code �quipement de TGV et Corail
        if (VerificationCodeEquipementResaven(_pSegTemp[lis]->getTypeEquipement()) == 0){continue;}
        // Fin correction ano 98698
        YmDate ldate_depart(_pSegTemp[lis]->getDateDepart());
//MHUE 11/09/06
        string szdate_depart_tr;
        ldate_depart.PrintFrench(szdate_depart_tr);
        TrLeg aTrLeg=g_MapTrLeg.InMap(_pSegTemp[lis]->getTrancheNo(),_pSegTemp[lis]->getOrigineSeg(),
                                      _pSegTemp[lis]->getDestinationSeg(), (char *)szdate_depart_tr.data());
        if (aTrLeg.indic_tgv!=2)
           {
		
           szdate_depart_tr = aTrLeg.dpt_date_tr;
           if (YmDate(szdate_depart_tr) < g_dateMinTranche ) continue;
           }
//MHUE 11/09/06

        // recherche du nest level du segment en fonction de son Origine, sa Destination,
        // sa classe de service, sa Date de depart, la date du jour et le numero de tranche

		/*JRO IC SRO DT 23805 On accepte les trains sans classe de controles
        if (_pSegTemp[lis]->getNestLevel() == -1)
        {
            // La valeur du nest level n'est pas trouvee, on ne prend pas le segment
            continue;

        }
		*/
        // Si le canal reservation n'est pas nul et n'existe pas dans la Map DeviceType =>emission message
        if (! _pSegTemp[lis]->getCanalReservation().empty()) {
            if (!g_MapDeviceType.isInMap(_pSegTemp[lis]->getCanalReservation())) {
                sprintf(sMessText,"Fichier : %s (ligne %d), TCN %d : canal de reservation %s inconnu pour segment no %d",(char *)_NomFichier_court,iNumLigne04,_pTcn->getTcnNumber(),_pSegTemp[lis]->getCanalReservation().c_str(),_pSegTemp[lis]->getSegmentNum());
                rMess.envoiMess(863,(char *)sMessText,YmRevMess::WARN);
            }
        }

        // affectation au segment de la valeur du pointeur du PassagerParcours
        _pSegTemp[lis]->setPPassagersParcoursCourant(_pPassagersParcours);

        // DT33254 affectation des variables Booking
        _pSegTemp[lis]->setPalierBooking(_pPassagersParcours->getPalierBooking());
        _pSegTemp[lis]->setUpgHan(_pPassagersParcours->getUpgHan());
        // affectation au segment de la valeur de classe de service memorisee
        _pSegTemp[lis]->setClassOfService(_pPassagersParcours->getClassOfService());

        // affectation au TarifType du Segments de la valeur du TarifType du PassagersParcours
        _pSegTemp[lis]->setTarifType(_pPassagersParcours->getTarifType());

        // comparaison de la date de depart du segment par rapport a la date Max des segments du Tcn
        lstrDateDepart=_pSegTemp[lis]->getDateDepart();
		
        if (_lDateMax < atol(lstrDateDepart.data())) {
			
            _lDateMax = atol(lstrDateDepart.data());
            _strDateMax=lstrDateDepart;
        }

        // calcul du revenu du segment _pSegTemp[lis]
        // Si segment Train-Auto les nombre de passagers du PassagersParcours est egal au nombre de MontantResa<>0 
        // dans la ligne de sous-structure 10 ( calcul dans la fonction initSegment10() de la classe YmRevSegments)
        
        if  (_pSegTemp[lis]->getSegAuto() == 1) _pPassagersParcours->setNombrePassagers(_pSegTemp[lis]->getNbVehic());
        _pSegTemp[lis]->calculRevenu(ldMontantTaxation,_iTotalDistanceSegmentsParcours,liSegEff,_pPassagersParcours->getNombrePassagers(),pTypePassager);

        // rajout du Segment dans la liste des Segments du PassagerParcours
        _pPassagersParcours->ajouterSegments(_pSegTemp[lis]);
    }
 
}  

void YmRevEcrireVec::InitialiserMembresPourLectureFichierResaven() {
                                  
  // pointeurs sur les classes Tcn, PassagersParcours, Annulation et Retraits
 
  this->_pTcn=NULL;
  this->_pPassagersParcours=NULL;
  this->_pSegments=NULL;
  this->_pAnnulation=NULL;
  this->_pRetraits=NULL;

  // tableau temporaire de stockage des pointeurs 'Segments' a rattacher a un 'PassagersParcours' d'un 'Tcn'
  this->_iSegTemp=0;
  this->_iNbPassagersParcours=0;
  this->_iTotalDistanceSegmentsParcours=0;
  this->_lDateMax=0;
  this->_strDateMax.clear();
  this->InitialiserTableauPointeursSegments();
}


void YmRevEcrireVec::InitialiserTableauPointeursSegments() {

	
  for (int i=0;i<_iSegTemp;i++) {
      delete _pSegTemp[i];
      _pSegTemp[i]=NULL;
  }
  _iSegTemp=0;
  _iTotalDistanceSegmentsParcours=0;
  _iAuto=0;

}

// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
//void YmRevEcrireVec::GestionFinTcn(int struc_prec,char * sLigne04,int iNumLigne04,int iNumLigne00) 
//DT33254 l'ajout du param�tre StructBooking
void YmRevEcrireVec::GestionFinTcn(int struc_prec,char * sLigne04,int iNumLigne04,int iNumLigne00, long numPalierSegment, Booking StructBooking)
{
	
    // Si le numero de structure de la ligne precedente est 04, generation d'instance de PassagerParcours
    if (struc_prec == 4) {
		//Si c'est un train auto, alors c'est incomplet et on supprimer le segment pr�cedent
		// TDR - Anomalie 90705 Segments dont le type d'�quipement n'est ni CORAIL, ni TGV
        if ((/*_iSegTemp > 0 && */!_isTGVorCORAIL) || _iAuto)
		{
			this->AnnulationSegmentTemporairePrecedent();
		}
		// HRI, 05/03/2015 : on a besoin du palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
        //this->TraiterPassagersParcoursSansInternational(sLigne04,iNumLigne04);
        //DT33254 l'ajout du param�tre StructBooking
		this->TraiterPassagersParcoursSansInternational(sLigne04,iNumLigne04,numPalierSegment, StructBooking);
    }
   
    // TDR - Anomalie 90705 Si le tcn n'a pas de train TGV ou Corail il n'est pas trait�
	if (!_isTCNHasTGVorCORAIL) 
	{
		delete _pTcn;
    }
	else
	{
		_isTCNHasTGVorCORAIL = false;
		
		if (_iNbPassagersParcours == 0) 
		{
			delete _pTcn;
		}

		else
		{
			
			if (! _strDateMax.empty())
				{
			
					YmDate ldate_tcn(_strDateMax);
					// La date Max des segments du Tcn doit etre >= date de l'entete du fichier moins 3 jours
					// Sinon le Tcn est abandonne
					YmDate ldate_ref(_strDateEntete);
					ldate_ref-=3;
					//
					// PLF 12/12/2006 : Rejet du Tcn quand la date Max de ses segments est < g_dateMinTranche
					// (le filtre existe deja au niveau du controle des segments mais uniquement pour un indicateur TGV du leg <> 2)
					//
					if (ldate_tcn < g_dateMinTranche) 
					{
						
						delete _pTcn;
					}
					else
					{

						if (ldate_tcn >= ldate_ref)
						{
                
							// affectation de la valeur de DateMaxDepart du Tcn
							_pTcn->setDateMaxDepart( _strDateMax);
							// rajout du pointeur Tcn dans le vecteur des pointeurs Tcn
							_pTcn->alimenterVecteurTcn();
							
						}
						else
						{
								delete _pTcn;
						}
					}
				}
			else 
			{
					
			//JRO IC SRO DT 23805
						// rajout du pointeur Tcn dans le vecteur des pointeurs Tcn
						_pTcn->alimenterVecteurTcn();
						

						//cout<<"TCN rejete : date MAX vide"<<endl;
						//delete _pTcn;

			}
			
		}
	}
}

void YmRevEcrireVec::AnnulationSegmentTemporairePrecedent()
{
	
	
    _pSegTemp[_iSegTemp-1]=NULL;
    _iSegTemp--;
    _iTotalDistanceSegmentsParcours-= _pSegments->getLongueurSeg();
    _iAuto=0;
    delete _pSegments;
	_pSegments = NULL;
}



void YmRevEcrireVec::InitialisationNouveauTcn(char *sLigne)
{

    YmRevMess rMess((char*)"batch_unitaire", (char*)"YmRevEcrireVec",(char*)"InitialisationNouveauTcn",(char*)"revenus");
    char sMessText[180];
    string unknown = string("???");
    _pTcn = new YmRevTcn();
    _pTcn->initTcn(sLigne);

    // Si la date et heure de paiement sont a null, on leur affecte la date et heure de la premiere ligne du fichier
    if (_pTcn->getDatePaiement().empty()) _pTcn->setDatePaiement(_strDateEntete);
    if (_pTcn->getHeurePaiement().empty()) _pTcn->setHeurePaiement(_strHeureEntete);

    // Le canal de paiement obligatoire dans la table REV_TCN est initialise par defaut a '???'
    if (_pTcn->getCanalPaiement().empty()) _pTcn->setCanalPaiement(unknown);
    else
    {
       // Si le canal de paiement n'existe pas dans la Map DeviceType => emission message
       if (!g_MapDeviceType.isInMap(_pTcn->getCanalPaiement())) {
           sprintf(sMessText,"Fichier : %s (ligne %d), TCN %d : canal de paiement %s inconnu",(char *)_NomFichier_court,_iNumeroLigneFichier,_pTcn->getTcnNumber(),_pTcn->getCanalPaiement().c_str());
           rMess.envoiMess(1001,(char *)sMessText,YmRevMess::WARN);
      }

    }
    // Si le canal emission n'est pas nul et n'existe pas dans la Map DeviceType emission d'un message
    if (! _pTcn->getCanalEmission().empty()) {
        if (!g_MapDeviceType.isInMap(_pTcn->getCanalEmission())) {
            sprintf(sMessText,"Fichier : %s (ligne %d), TCN %d : canal emission %s inconnu",(char *)_NomFichier_court,_iNumeroLigneFichier,_pTcn->getTcnNumber(),_pTcn->getCanalEmission().c_str());
            rMess.envoiMess(1009,(char *)sMessText,YmRevMess::WARN);
        }

    } 

  

    // Reinitialisation du tableau temporaire des pointeurs segments
    InitialiserTableauPointeursSegments();

    //reinitialisation des variables date Max de depart des segments du Tcn,Distance totale des segments d'un PassagesrParcours, 
    // Nombre de passagers d'un PassagersParcours
    _lDateMax=0;
    _strDateMax.clear();
    _iTotalDistanceSegmentsParcours=0;
    _iNbPassagersParcours=0;

}

// HRI, 05/03/2015 : on recupere le palier du segment FCEN94-02 afin de l'utiliser pour FCEN04-04 (qui n'en dispose pas)
//void YmRevEcrireVec::InitialisationNouveauSegmentTemporaire(char *sLigne)
//DT33254 l'ajout du param�tre StructBooking
void YmRevEcrireVec::InitialisationNouveauSegmentTemporaire(char *sLigne, long& numpalierRet, Booking *StructBooking)
{
    _pSegments = new YmRevSegments();
    _pSegments->initSegments02(sLigne,g_InfoBatch.getAssocData ("DATE_BANG"));

    // Si la date de paiement est a null, on lui affecte la date de la premiere ligne du fichier
    if (_pSegments->getDatePaiement().empty()){
         _pSegments->setDatePaiement(_strDateEntete);
    }

    // Cumul de la distance du segment dans la distance totale du parcours
    _iTotalDistanceSegmentsParcours+= _pSegments->getLongueurSeg();
    
    // rajout du segment dans le tableau temporaire des segments
    _pSegTemp[_iSegTemp] = _pSegments;
	numpalierRet = _pSegments->getNumPalier();
	// R�cup�rer l'indicateur upghan et la classe de service booking ainsi que le palier booking

    StructBooking->numPalierBooking = _pSegments->getPalierBooking();
    StructBooking->classOfServiceBooking = _pSegments->getClasseBooking();
    StructBooking->UpgHan = _pSegments->getUpgHan();

    _iSegTemp++;
}



void YmRevEcrireVec::AffichageListesConstituees()
{
// Nombre de pointeurs des vecteurs globaux g_VTcn, g_VAnnulation et g_VRetraits
cout << "------------------------------------------------------------------------------" << endl;
cout << "Nombre de pointeurs des vecteurs globaux g_VTcn, g_VAnnulation et g_VRetraits " << endl;
cout << "------------------------------------------------------------------------------" << endl;
cout << endl;
// Afficher le nombre de pointeurs du vecteur des pointeurs Tcn
LOCK_MUTEX (&g_MutexVTcn);
cout << "Nombre de pointeurs du vecteur g_VTcn : " << g_VTcn.size() << endl;
UNLOCK_MUTEX (&g_MutexVTcn);
// afficher le nombre de pointeurs du vecteur des pointeurs Annulation
LOCK_MUTEX (&g_MutexVAnnulation);
cout << "Nombre de pointeurs du vecteur g_VAnnulation : " << g_VAnnulation.size() << endl;
UNLOCK_MUTEX (&g_MutexVAnnulation);
// afficher le nombre de pointeurs du vecteur des pointeurs Retraits
LOCK_MUTEX (&g_MutexVRetraits);
cout << "Nombre de pointeurs du vecteur g_VRetraits : " << g_VRetraits.size() << endl;
UNLOCK_MUTEX (&g_MutexVRetraits);
cout << endl;
// Nombre de tcn, PassagersParcours, Segments
cout << "---------------------------------------------" << endl;
cout << "Nombre de Tcn, PassagersParcours et Segments " << endl;
cout << "---------------------------------------------" << endl;
cout << endl;
int it=0;
int ip=0;
int is=0;
LOCK_MUTEX (&g_MutexVTcn);
for (int i=0;i< g_VTcn.size();i++) {
    it++;
    YmRevTcn* lpTcn=g_VTcn[i];

    list<YmRevPassagersParcours *> lpPassagersParcours=lpTcn->_listePassagersParcours;
    list<YmRevPassagersParcours *>::const_iterator j;

    for (j=lpPassagersParcours.begin();j != lpPassagersParcours.end(); j++) {
        ip++;
        YmRevPassagersParcours* lpPassagersParcours=*j;

        list<YmRevSegments *> lpSegments=lpPassagersParcours->_listeSegments;
        list<YmRevSegments *>::const_iterator k;
        for (k=lpSegments.begin();k != lpSegments.end(); k++) {
            is++;
        }
    }
}
UNLOCK_MUTEX(&g_MutexVTcn);
cout << "Nombre de Tcn               : " << it <<endl;
cout << "Nombre de PassagersParcours : " << ip <<endl;
cout << "Nombre de Segments          : " << is <<endl;

// Liste et caracteristiques pointees des pointeurs selectionnes Tcn->PassagersParcours->Segments
cout << "----------------------------------------------------------------------------------------------" << endl;
cout << "Liste et caracteristiques pointees des pointeurs selectionnes Tcn->PassagersParcours->Segments" << endl;
cout << "----------------------------------------------------------------------------------------------" << endl;

LOCK_MUTEX (&g_MutexVTcn);
for (int i=0;i< g_VTcn.size();i++) {
    cout << endl;
    cout << "adresse pointeur Tcn : " << g_VTcn[i] << endl;
    cout << "********************" <<endl;
    cout << "numero Tcn : " << g_VTcn[i]->getTcnNumber() << endl;
    cout << "date paiement : " << g_VTcn[i]->getDatePaiement() << endl;
    cout << "heure paiement : " << g_VTcn[i]->getHeurePaiement() << endl;
    cout << "canal Paiement : " << g_VTcn[i]->getCanalPaiement() <<endl;
    cout << "date emission physique : " << g_VTcn[i]->getDateEmissionPhysique() << endl;
    cout << "canal emission : " << g_VTcn[i]->getCanalEmission() << endl;
    cout << "dateMaxDepart : " << g_VTcn[i]->getDateMaxDepart() << endl;
    cout << "********************" <<endl;

    YmRevTcn* lpTcn=g_VTcn[i];
    list<YmRevPassagersParcours *> lpPassagersParcours=lpTcn->_listePassagersParcours;
    list<YmRevPassagersParcours *>::const_iterator j;

    for (j=lpPassagersParcours.begin();j != lpPassagersParcours.end(); j++) {
       cout << endl;
       cout << "    adresse pointeur PassagersParcours " << *j << endl;
       cout << "    =====================" <<endl;
       YmRevPassagersParcours* lpPassagersParcours=*j;
       cout << "    numero Tcn : " << lpPassagersParcours->getTcnNumber() << endl;
       cout << "    date paiement : " << lpPassagersParcours->getDatePaiement() << endl;
       cout << "    tarif type : " << lpPassagersParcours->getTarifType() << endl;
       cout << "    classe de service : " << lpPassagersParcours->getClassOfService() << endl;
	   cout << "    Palier : " << lpPassagersParcours->getNumPalier() << endl;
       cout << "    type passager : " << lpPassagersParcours->getPassagerType() << endl;
       cout << "    code tarif : " << lpPassagersParcours->getCodeTarif() << endl;
       cout << "    nombre passagers : " << lpPassagersParcours->getNombrePassagers() << endl;
       cout << "    montant unit transport : " << lpPassagersParcours->getMontantUnitTransport() << endl;
       cout << "    surtaxe origine : " << lpPassagersParcours->getSurtaxeOrigine() <<endl;
       cout << "    surtaxe destination : " << lpPassagersParcours->getSurtaxeDestination() << endl;
       cout << "    montant domestique : " << lpPassagersParcours->getMontantDomestique() << endl;
       cout << "    =====================" <<endl;
       list<YmRevSegments *> lpSegments=lpPassagersParcours->_listeSegments;
       list<YmRevSegments *>::const_iterator k;
       for (k=lpSegments.begin();k != lpSegments.end(); k++) {
           cout <<endl;
           cout << "        adresse pointeur Segments " << *k << endl;
           cout << "        --------------------" << endl;
           YmRevSegments* lpSegments=*k;
           cout << "        numero Tcn : " << lpSegments->getTcnNumber() << endl;
           cout << "        date paiement : " << lpSegments->getDatePaiement() << endl;
           cout << "        tarif type : " << lpSegments->getTarifType() << endl;
           cout << "        classe de service : " << lpSegments->getClassOfService() << endl;
           cout << "        numero segment : " << lpSegments->getSegmentNum() << endl;
           cout << "        origine segment : " << lpSegments->getOrigineSeg() << endl;
           cout << "        destination segment : " << lpSegments->getDestinationSeg() <<endl;
           cout << "        longueur segment : " << lpSegments->getLongueurSeg() << endl;
           cout << "        date resa : " << lpSegments->getDateResa() <<endl;
           cout << "        canal reservation : " <<  lpSegments->getCanalReservation() << endl;
           cout << "        nest level : " << lpSegments->getNestLevel() << endl;
           cout << "        train no : " << lpSegments->getTrainNo() << endl;
           cout << "        tranche no : " << lpSegments->getTrancheNo() << endl;
           cout << "        tranche carrier code : " << lpSegments->getTrancheCarrierCode() << endl;
           cout << "        date Depart : " << lpSegments->getDateDepart() << endl;
           cout << "        heure Depart : " << lpSegments->getHeureDepart() << endl;
           cout << "        date Arrivee : " << lpSegments->getDateArrivee() << endl;
           cout << "        heure Arrivee : " << lpSegments->getHeureArrivee() << endl;
		   //DM 7978 - JLA - Changement niveau social
           cout << "        Niveau Social : " << lpSegments->getNiveauSocial() << endl;
           cout << "        type equipement : " << lpSegments->getTypeEquipement() << endl;
           cout << "        montant Resa : " << lpSegments->getMontantResa() << endl;
           cout << "        montant Supplement : " << lpSegments->getMontantSupplement() << endl;
           cout << "        revenu : " << lpSegments->getRevenu() << endl;
           cout << "        scx : " << lpSegments->getScx() << endl;
           cout << "        --------------------" << endl;
       }
    }
}
UNLOCK_MUTEX(&g_MutexVTcn);
// Liste et caracteristiques pointees des pointeurs retenus dans g_VAnnulation
cout << endl;
cout << "--------------------------------------------------------------------------------------" << endl;
cout << "Liste et caracteristiques pointees des pointeurs retenus dans le vecteur g_VAnnulation" << endl;
cout << "--------------------------------------------------------------------------------------" << endl;
LOCK_MUTEX (&g_MutexVAnnulation);
for (int i=0;i< g_VAnnulation.size();i++) {
    cout << endl;
    cout << "adresse pointeur Annulation : " << g_VAnnulation[i] << endl;
    cout << "********************" <<endl;
    cout << "Tcn annule : " << g_VAnnulation[i]->getTcnAnnule() << endl;
    cout << "date tcn : " << g_VAnnulation[i]->getDateTcn() << endl;
    cout << "date operation : " << g_VAnnulation[i]->getDateOperation() << endl;
    cout << "heure operation : " << g_VAnnulation[i]->getHeureOperation() << endl;
    cout << "montant tcn : " << g_VAnnulation[i]->getMontantTcn() << endl;
    cout << "montant rembourse : " << g_VAnnulation[i]->getMontantRembourse() << endl;
    cout << "montant retenues : " << g_VAnnulation[i]->getMontantRetenues() << endl;
    cout << "station origine : " << g_VAnnulation[i]->getStationOrigine() << endl;
    cout << "station destination : " << g_VAnnulation[i]->getStationDestination() << endl;
    cout << "********************" <<endl;
}
UNLOCK_MUTEX(&g_MutexVAnnulation);

// Liste et caracteristiques pointees des pointeurs retenus dans g_VRetraits
cout << endl;
cout << "------------------------------------------------------------------------------------" << endl;
cout << "Liste et caracteristiques pointees des pointeurs retenus dans le vecteur g_VRetraits" << endl;
cout << "------------------------------------------------------------------------------------" << endl;
LOCK_MUTEX (&g_MutexVRetraits);
for (int i=0;i< g_VRetraits.size();i++) {
    cout << endl;
    cout << "adresse pointeur Retraits : " << g_VRetraits[i] << endl;
    cout << "********************" <<endl;
    cout << "numero Tcn : " << g_VRetraits[i]->getTcnNumber() << endl;
    cout << "date transaction : " << g_VRetraits[i]->getDateTransaction() << endl;
    cout << "heure transaction : " << g_VRetraits[i]->getHeureTransaction() << endl;
    cout << "canal emission : " << g_VRetraits[i]->getCanalEmission() <<endl;
    cout << "********************" <<endl;
}
UNLOCK_MUTEX(&g_MutexVRetraits);

return ;
}
char* YmRevEcrireVec::existenceCompart(char c)
{
 int i;
 for (i=0;i<strlen(g_CmptList);i++)
 {
     if (g_CmptList[i] == c)
        return(g_CmptList);
 }
 return(NULL);
}

