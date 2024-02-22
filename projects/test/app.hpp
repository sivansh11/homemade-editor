#ifndef APP_HPP
#define APP_HPP

namespace app {

class app_t {
public:
    static app_t *create();
    static void destroy(app_t *app);

    void draw(float dt);

private:

};

} // namespace app

#endif