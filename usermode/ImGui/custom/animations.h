//First time using this method
#include <string>
#include <map>
#include "../imgui.h"

enum AnimationTypes { INTERP };

inline std::string pChar(std::string first_, std::string second_) {
    return (first_ + second_);
}

inline float Animate(const char* label, const char* second_label, bool if_, float Maximal, float Speed, int type) {

    auto ID = ImGui::GetID(pChar(label, second_label).c_str());

    static std::map<ImGuiID, float> pValue;

    auto this_e = pValue.find(ID);

    if (this_e == pValue.end()) {
        pValue.insert({ ID, 0.f });
        this_e = pValue.find(ID);
    }

    switch (type) {

    case INTERP: {
        if (if_) //do
            this_e->second += (Maximal - this_e->second) / Speed;
        else
            this_e->second -= (0 + this_e->second) / Speed;
    }
               break;
    }
    {
        if (this_e->second < 0.f)
            this_e->second = 0.f;
        else if (this_e->second > Maximal)
            this_e->second = Maximal;
    }

    return this_e->second;

}