#include <G4Box.hh>
#include <G4Tubs.hh>
#include <G4NistManager.hh>
#include <G4ParticleGun.hh>
#include <G4ParticleTable.hh>
#include <G4ParticleDefinition.hh>
#include <G4PVPlacement.hh>
#include <G4RunManager.hh>
#include <G4Event.hh>
#include <G4SDManager.hh>
#include <G4VisExecutive.hh>
#include <G4VisManager.hh>
#include <G4VSensitiveDetector.hh>
#include <G4VUserDetectorConstruction.hh>
#include <G4VUserPrimaryGeneratorAction.hh>
#include <G4UIExecutive.hh>
#include <G4UImanager.hh>
#include <G4UIsession.hh>
#include <G4UserEventAction.hh>
#include <QGSP_BERT_HP.hh>
#include <G4RandomDirection.hh>
using namespace std;
using CLHEP::MeV;
using CLHEP::cm;
using CLHEP::mm;
using CLHEP::degree;
 

struct det_constr: G4VUserDetectorConstruction
{
    G4VPhysicalVolume* Construct ()
    {
        G4Box *world_box = new G4Box ("world_box", 500*mm, 500*mm, 500*mm);
        G4Material *water = G4NistManager::Instance()->FindOrBuildMaterial ("G4_WATER");
        G4LogicalVolume *world_logical_volume = new G4LogicalVolume (world_box, water, "world_logical_vol");
        G4VPhysicalVolume *world_physical_volume = new G4PVPlacement (NULL, G4ThreeVector(), world_logical_volume, 
        "world_physical_vol", NULL, false, NULL);

        return world_physical_volume;
    }
}


int main()
{
    G4RunManager *RM = new G4RunManager;
    return 0;    
}