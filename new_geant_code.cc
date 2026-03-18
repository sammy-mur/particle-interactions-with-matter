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
#include <FTFP_BERT.hh>
#include <G4RandomDirection.hh>
#include <TFile.h>
#include <TNtuple.h>
using namespace std;
using CLHEP::MeV;
using CLHEP::cm;
using CLHEP::mm;
using CLHEP::degree;

class sensitive_detector : public G4VSensitiveDetector
{
private:
  double edep;
  TNtuple* ntuple; 
public:
  sensitive_detector (G4String name) : G4VSensitiveDetector (name)
  {
    ntuple = new TNtuple("hits", "detector hits", "edep");
  }

   virtual ~sensitive_detector() {
    if (ntuple) {
      ntuple->Write();   
      delete ntuple;
    }
  }

  virtual void Initialize (G4HCofThisEvent*)
  {
    edep = 0.0;
  }

  G4bool ProcessHits (G4Step *step, G4TouchableHistory*)
  {
    edep += step->GetTotalEnergyDeposit ();
    return true;
  }
  
  virtual void EndOfEvent (G4HCofThisEvent*)
  {
    int e = G4RunManager::GetRunManager ()->GetCurrentEvent ()->GetEventID ();
    if (edep > 0)
      {
        G4cout << ">>> event " << e
               << " energy deposition " << edep / MeV << " MeV"
               << endl;
        ntuple->Fill(edep / MeV);
      }
  }
};
 

struct det_constr: G4VUserDetectorConstruction
{
    G4VPhysicalVolume* Construct ()
    {
        G4Box *world_box = new G4Box ("world_box", 500*mm, 500*mm, 500*mm);
        G4Material *water = G4NistManager::Instance()->FindOrBuildMaterial ("G4_WATER");
        G4LogicalVolume *world_logical_volume = new G4LogicalVolume (world_box, water, "world_logical_vol");
        G4VPhysicalVolume *world_physical_volume = new G4PVPlacement (0, G4ThreeVector(), world_logical_volume, "world_physical_vol", 0, false, 0);

      

        sensitive_detector *detector = new sensitive_detector ("detector");
        G4SDManager::GetSDMpointer ()->AddNewDetector (detector);
        world_logical_volume->SetSensitiveDetector (detector);
        
        return world_physical_volume;
    }
};

struct pga: G4VUserPrimaryGeneratorAction
{
    G4ParticleGun *particle_gun;
    pga()
    {
        this->particle_gun = new G4ParticleGun(1);
    }

    ~pga()
    {
        delete this->particle_gun;
    
    }

    void GeneratePrimaries(G4Event *event)
    {
        this->particle_gun->GeneratePrimaryVertex(event);
    }

};


int main(int argc, char *argv [])
{
    TFile* file = new TFile("filename.root", "RECREATE");

    G4RunManager *rm = new G4RunManager;
    rm->SetUserInitialization(new det_constr);
    rm->SetUserInitialization(new FTFP_BERT);
    rm->SetUserAction (new pga);
    rm->Initialize();

    G4UImanager *ui = G4UImanager::GetUIpointer ();

    G4VisManager *vm = new G4VisExecutive;
  vm->Initialize ();

  G4UIExecutive *uie = new G4UIExecutive (argc, argv);

  ui->ApplyCommand ("/control/execute vis.mac");
  uie->SessionStart ();

  delete rm;
  //file->Close();
  delete file;
    return 0;    
}
