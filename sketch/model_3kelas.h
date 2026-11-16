#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class PenyiramanModel3Kelas {
                public:
                    /**
                    * Predict class for features vector
                    */
                    int predict(float *x) {
                        if (x[0] <= 43.190425872802734) {
                            if (x[1] <= 31.998613357543945) {
                                if (x[0] <= 40.221778869628906) {
                                    return 1;
                                }

                                else {
                                    if (x[1] <= 29.430326461791992) {
                                        return 0;
                                    }

                                    else {
                                        return 1;
                                    }
                                }
                            }

                            else {
                                if (x[0] <= 39.957279205322266) {
                                    return 2;
                                }

                                else {
                                    return 1;
                                }
                            }
                        }

                        else {
                            if (x[0] <= 59.96682167053223) {
                                if (x[1] <= 29.969441413879395) {
                                    return 0;
                                }

                                else {
                                    return 1;
                                }
                            }

                            else {
                                return 0;
                            }
                        }
                    }

                protected:
                };
            }
        }
    }