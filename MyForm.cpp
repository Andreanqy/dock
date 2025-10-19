#include "MyForm.h"

[STAThread]
void Main(array<String^>^ args) {
	Application::EnableVisualStyles();
	Application::SetCompatibleTextRenderingDefault(false);
	Project13::MyForm form;
	Application::Run(% form);
}
