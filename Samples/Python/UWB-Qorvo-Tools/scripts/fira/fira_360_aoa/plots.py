import matplotlib.pyplot as plt


class plotLUT:

    @staticmethod
    def plot_lut(
        lut, title, invert0=False, invert1=False, invert2=False, no_time=False
    ):
        fig = plt.figure()
        if not no_time:
            angle = [k[0] for k in lut]
        else:
            angle = list(range(len(lut)))
        pdoa1 = [k[1] if not invert0 else -k[1] for k in lut]
        pdoa2 = [k[2] if not invert1 else -k[2] for k in lut]
        pdoa3 = [k[3] if not invert2 else -k[3] for k in lut]

        axes = fig.add_subplot(111)
        axes.clear()
        axes.plot(angle, pdoa1, label="PDoA 1")
        axes.plot(angle, pdoa2, label="PDoA 2")
        axes.plot(angle, pdoa3, label="PDoA 3")
        axes.legend()
        axes.grid()
        plt.title(title)
        plt.show()
